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

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);
static NS_DEFINE_CID(kScriptSecurityManagerCID, NS_SCRIPTSECURITYMANAGER_CID);

#if defined(PR_LOGGING)
#include "nsXPIDLString.h"
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

    NS_IMETHOD OnStopRequest(nsIChannel* jarCacheTransport, 
                             nsISupports* context, 
                             nsresult status, 
                             const PRUnichar* aMsg) {
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
                        (const char*)jarURLStr, status));
            }
        }
#endif
        if (NS_SUCCEEDED(status) && mJARChannel->mJarCacheTransport) {
            NS_ASSERTION(jarCacheTransport == (mJARChannel->mJarCacheTransport).get(),
                         "wrong transport");
            // after successfully downloading the jar file to the cache,
            // start the extraction process:
            nsCOMPtr<nsIFileChannel> jarCacheFile;
            rv = NS_NewLocalFileChannel(getter_AddRefs(jarCacheFile),
                                        mJarCacheFile, 
                                        PR_RDONLY,
                                        0);
            if (NS_FAILED(rv)) return rv;
            rv = jarCacheFile->SetLoadGroup(mJARChannel->mLoadGroup);
            if (NS_FAILED(rv)) return rv;
            rv = jarCacheFile->SetBufferSegmentSize(mJARChannel->mBufferSegmentSize);
            if (NS_FAILED(rv)) return rv;
            rv = jarCacheFile->SetBufferMaxSize(mJARChannel->mBufferMaxSize);
            if (NS_FAILED(rv)) return rv;
            rv = jarCacheFile->SetLoadAttributes(mJARChannel->mLoadAttributes);
            if (NS_FAILED(rv)) return rv;
            rv = jarCacheFile->SetNotificationCallbacks(mJARChannel->mCallbacks);
            if (NS_FAILED(rv)) return rv;

            mJARChannel->SetJARBaseFile(jarCacheFile);
            rv = mOnJARFileAvailable(mJARChannel, mClosure);
        }
        mJARChannel->mJarCacheTransport = nsnull;
        return rv;
    }

    nsJARDownloadObserver(nsIFile* jarCacheFile, nsJARChannel* jarChannel,
                          OnJARFileAvailableFun onJARFileAvailable, 
                          void* closure)
        : mJarCacheFile(jarCacheFile),
          mJARChannel(jarChannel),
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
    nsCOMPtr<nsIFile>           mJarCacheFile;
    nsJARChannel*               mJARChannel;
    OnJARFileAvailableFun       mOnJARFileAvailable;
    void*                       mClosure;
};

NS_IMPL_ISUPPORTS1(nsJARDownloadObserver, nsIStreamObserver)

////////////////////////////////////////////////////////////////////////////////

nsJARChannel::nsJARChannel()
    : mContentType(nsnull),
      mContentLength(-1),
      mStartPosition(0),
      mReadCount(-1),
      mJAREntry(nsnull),
      mMonitor(nsnull),
      mStatus(NS_OK)
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

NS_IMPL_ISUPPORTS6(nsJARChannel,
                   nsIJARChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamObserver,
                   nsIStreamListener,
                   nsIFileSystem)

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

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods

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
OpenJARElement(nsJARChannel* channel, void* closure)
{
    nsresult rv;
    nsIInputStream* *result = (nsIInputStream**)closure;
    nsAutoCMonitor mon(channel);
    rv = channel->Open(nsnull, nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = channel->GetInputStream(result);
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

NS_IMETHODIMP
nsJARChannel::AsyncOpen(nsIStreamObserver* observer, nsISupports* ctxt)
{
	NS_NOTREACHED("nsJARChannel::AsyncOpen");
    return NS_ERROR_NOT_IMPLEMENTED;
}

static nsresult
ReadJARElement(nsJARChannel* channel, void* closure)
{
    nsresult rv;
    rv = channel->AsyncReadJARElement();
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
    nsresult rv;

#ifdef PR_LOGGING
    nsXPIDLCString jarURLStr;
    mURI->GetSpec(getter_Copies(jarURLStr));
    PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
           ("nsJarProtocol: EnsureJARFileAvailable %s", (const char*)jarURLStr));
#endif

    rv = mURI->GetJARFile(getter_AddRefs(mJARBaseURI));
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetJAREntry(&mJAREntry);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> jarBaseChannel;
    rv = NS_OpenURI(getter_AddRefs(jarBaseChannel), mJARBaseURI, nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = jarBaseChannel->SetLoadGroup(mLoadGroup);
    if (NS_FAILED(rv)) return rv;
    rv = jarBaseChannel->SetLoadAttributes(mLoadAttributes);
    if (NS_FAILED(rv)) return rv;
    rv = jarBaseChannel->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup)
        (void)mLoadGroup->AddChannel(this, nsnull);

//    mJARBaseFile = do_QueryInterface(jarBaseChannel, &rv);

    PRBool shouldCache;
    rv = jarBaseChannel->GetShouldCache(&shouldCache);

    if (NS_SUCCEEDED(rv) && !shouldCache) {
        // then we've already got a local jar file -- no need to download it
        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: extracting local jar file %s", (const char*)jarURLStr));
        rv = onJARFileAvailable(this, closure);
    }
    else {
        // otherwise, we need to download the jar file

        nsCOMPtr<nsIFile> jarCacheFile;
        rv = GetCacheFile(getter_AddRefs(jarCacheFile));
        if (NS_FAILED(rv)) return rv;
        
        PRBool filePresent;
        
		rv = jarCacheFile->IsFile(&filePresent);
        
        if (NS_SUCCEEDED(rv) && filePresent)
        {
	        // then we've already got the file in the local cache -- no need to download it
            rv = NS_NewLocalFileChannel(getter_AddRefs(mJARBaseFile),
                                        jarCacheFile, 
                                        PR_RDONLY,
                                        0);
            if (NS_FAILED(rv)) return rv;
            rv = mJARBaseFile->SetBufferSegmentSize(mBufferSegmentSize);
            if (NS_FAILED(rv)) return rv;
            rv = mJARBaseFile->SetBufferMaxSize(mBufferMaxSize);
            if (NS_FAILED(rv)) return rv;

            PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
                   ("nsJarProtocol: jar file already in cache %s", (const char*)jarURLStr));
            rv = onJARFileAvailable(this, closure);
			return rv;
		}

        NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsAutoMonitor monitor(mMonitor);

        // use a file transport to serve as a data pump for the download (done
        // on some other thread)
        nsCOMPtr<nsIChannel> jarCacheTransport;
        rv = fts->CreateTransport(jarCacheFile, PR_RDONLY, 0, 
                                  getter_AddRefs(mJarCacheTransport));
        if (NS_FAILED(rv)) return rv;
        rv = mJarCacheTransport->SetBufferSegmentSize(mBufferSegmentSize);
        if (NS_FAILED(rv)) return rv;
        rv = mJarCacheTransport->SetBufferMaxSize(mBufferMaxSize);
        if (NS_FAILED(rv)) return rv;

        rv = mJarCacheTransport->SetNotificationCallbacks(mCallbacks);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIStreamObserver> downloadObserver = 
            new nsJARDownloadObserver(jarCacheFile, this, onJARFileAvailable, closure);
        if (downloadObserver == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: downloading jar file %s", (const char*)jarURLStr));
        nsCOMPtr<nsIInputStream> jarBaseIn;
        rv = jarBaseChannel->OpenInputStream(getter_AddRefs(jarBaseIn));
        if (NS_FAILED(rv)) return rv;

        rv = mJarCacheTransport->AsyncWrite(jarBaseIn, nsnull, downloadObserver);
    }
    return rv;
}

nsresult 
nsJARChannel::GetCacheFile(nsIFile* *cacheFile)
{
    // XXX change later to use the real network cache
    nsresult rv;

    nsCOMPtr<nsIFile> jarCacheFile;
    rv = NS_GetSpecialDirectory("xpcom.currentProcess.componentDirectory",
                                getter_AddRefs(jarCacheFile));
    if (NS_FAILED(rv)) return rv;

    jarCacheFile->Append("jarCache");

    PRBool exists;
    rv = jarCacheFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists) {
        rv = jarCacheFile->Create(nsIFile::DIRECTORY_TYPE, 0664);
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
    return rv;
}

nsresult
nsJARChannel::AsyncReadJARElement()
{
    nsresult rv;

    nsAutoMonitor monitor(mMonitor);

    NS_ASSERTION(mJARBaseFile, "mJARBaseFile is null");

    if (mLoadGroup) {
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

        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransportFromFileSystem(this, 
                                            getter_AddRefs(mJarExtractionTransport));
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetBufferSegmentSize(mBufferSegmentSize);
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetBufferMaxSize(mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    rv = mJarExtractionTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;

#ifdef PR_LOGGING
    nsXPIDLCString jarURLStr;
    mURI->GetSpec(getter_Copies(jarURLStr));
    PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
           ("nsJarProtocol: AsyncRead jar entry %s", (const char*)jarURLStr));
#endif
    rv = mJarExtractionTransport->SetTransferOffset(mStartPosition);
    if (NS_FAILED(rv)) return rv;
    rv = mJarExtractionTransport->SetTransferCount(mReadCount);
    if (NS_FAILED(rv)) return rv;
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
nsJARChannel::GetShouldCache(PRBool *aShouldCache)
{
    // Jar files report that you shouldn't cache them because this is really
    // a question about the jar entry, and the jar entry is always in a jar
    // file on disk.
    *aShouldCache = PR_FALSE;
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
    if (!mOwner)
    {
        nsCOMPtr<nsIPrincipal> certificate;
        PRInt16 result;
        nsresult rv = mJAR->GetCertificatePrincipal(mJAREntry, 
                                                    getter_AddRefs(certificate),
                                                    &result);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
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
            NS_ASSERTION(NS_SUCCEEDED(rv), 
                         "Certificate principal is not an aggregate");
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
nsJARChannel::OnStopRequest(nsIChannel* jarExtractionTransport,
                            nsISupports* context, 
                            nsresult status, 
                            const PRUnichar* aMsg)
{
    nsresult rv;
#ifdef PR_LOGGING
    nsCOMPtr<nsIURI> jarURI;
    nsXPIDLCString jarURLStr;
    rv = mURI->GetSpec(getter_Copies(jarURLStr));
    if (NS_SUCCEEDED(rv)) {
        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: jar extraction complete %s status=%x",
                (const char*)jarURLStr, status));
    }
#endif
    rv = mUserListener->OnStopRequest(this, mUserContext, status, aMsg);

    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, status, aMsg);
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
// nsIFileSystem methods:

NS_IMETHODIMP
nsJARChannel::Open(char* *contentType, PRInt32 *contentLength) 
{
	nsresult rv;
    NS_ASSERTION(mJARBaseFile, "mJARBaseFile is null");

	rv = nsComponentManager::CreateInstance(kZipReaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIZipReader),
                                            getter_AddRefs(mJAR));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> fs;
    rv = mJARBaseFile->GetFile(getter_AddRefs(fs));
    if (NS_FAILED(rv)) return rv; 

	rv = mJAR->Init(fs);
    if (NS_FAILED(rv)) return rv; 

	rv = mJAR->Open();
    if (NS_FAILED(rv)) return rv; 

    // If this fails, GetOwner will fail, but otherwise we can continue.
	mJAR->ParseManifest();

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
    mJAR = null_nsCOMPtr();
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
	return mJAR->GetInputStream(mJAREntry, aInputStream);
}
 
NS_IMETHODIMP
nsJARChannel::GetOutputStream(nsIOutputStream* *aOutputStream) 
{
	NS_NOTREACHED("nsJARChannel::GetOutputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
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
