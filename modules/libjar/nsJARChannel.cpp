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
 * Copyright (C) 1998,2000 Netscape Communications Corporation. All Rights
 * Reserved.
 *
 */

#include "nsNetUtil.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsJARChannel.h"
#include "nsCRT.h"
#include "nsIFileTransportService.h"
#include "nsIURI.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsAutoLock.h"
#include "nsIFileStreams.h"
#include "nsMimeTypes.h"
#include "nsScriptSecurityManager.h"
#include "nsIAggregatePrincipal.h"
#include "nsIProgressEventSink.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIJAR.h"
#include "prthread.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);
static NS_DEFINE_CID(kScriptSecurityManagerCID, NS_SCRIPTSECURITYMANAGER_CID);

#if defined(PR_LOGGING)
//
// Log module for JarChannel logging...
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

#define NS_DEFAULT_JAR_BUFFER_SEGMENT_SIZE      (16*1024)
#define NS_DEFAULT_JAR_BUFFER_MAX_SIZE          (256*1024)

nsJARChannel::nsJARChannel()
    : mLoadFlags(LOAD_NORMAL)
    , mContentType(nsnull)
    , mContentLength(-1)
    , mJAREntry(nsnull)
    , mStatus(NS_OK)
#ifdef DEBUG
    , mInitiator(nsnull)
#endif
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
}

NS_IMPL_THREADSAFE_ISUPPORTS7(nsJARChannel,
                              nsIJARChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIRequestObserver,
                              nsIStreamListener,
                              nsIStreamIO,
                              nsIDownloadObserver)

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
    *result = ToNewUnicode(name);
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
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(), "wrong thread");
#endif
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv = NS_OK;

    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Cancel(status);
        mJarExtractionTransport = nsnull;
    }

    mStatus = status;
    return rv;
}

NS_IMETHODIMP
nsJARChannel::Suspend()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(), "wrong thread");
#endif
    nsresult rv = NS_OK;

    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Suspend();
    }

    return rv;
}

NS_IMETHODIMP
nsJARChannel::Resume()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(), "wrong thread");
#endif
    nsresult rv = NS_OK;

    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Resume();
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods

NS_IMETHODIMP
nsJARChannel::GetOriginalURI(nsIURI* *aOriginalURI)
{
    if (mOriginalURI)
        *aOriginalURI = mOriginalURI;
    else
       *aOriginalURI = NS_STATIC_CAST(nsIURI*, mURI);

    NS_IF_ADDREF(*aOriginalURI);
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

nsresult
nsJARChannel::OpenJARElement()
{
    nsresult rv;
    nsAutoCMonitor mon(this);
    rv = Open(nsnull, nsnull);
    if (NS_SUCCEEDED(rv))
        rv = GetInputStream(getter_AddRefs(mSynchronousInputStream));
    mon.Notify();       // wake up nsIChannel::Open
    return rv;
}

NS_IMETHODIMP
nsJARChannel::Open(nsIInputStream* *result)
{
    nsAutoCMonitor mon(this);
    nsresult rv;
    mSynchronousRead = PR_TRUE;
    rv = EnsureJARFileAvailable();
    if (NS_FAILED(rv)) return rv;
    if (mSynchronousInputStream == nsnull) 
        mon.Wait();
    if (!mSynchronousInputStream)
        return NS_ERROR_FAILURE;

    *result = mSynchronousInputStream; // Result of GetInputStream called on transport thread
    NS_ADDREF(*result);
    mSynchronousInputStream = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::AsyncOpen(nsIStreamListener* listener, nsISupports* ctxt)
{
    nsresult rv;

#ifdef DEBUG
    mInitiator = PR_CurrentThread();
#endif

    mUserContext = ctxt;
    mUserListener = listener;
    mSynchronousRead = PR_FALSE;

    if (mLoadGroup) {
        rv = mLoadGroup->AddRequest(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    rv = EnsureJARFileAvailable();
    if (NS_FAILED(rv) && mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, rv);
    return rv;
}

nsresult
nsJARChannel::EnsureJARFileAvailable()
{
    nsresult rv;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gJarProtocolLog, PR_LOG_DEBUG)) {
        nsXPIDLCString jarURLStr;
        mURI->GetSpec(getter_Copies(jarURLStr));
        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: EnsureJARFileAvailable %s", (const char*)jarURLStr));
    }
#endif

    rv = mURI->GetJARFile(getter_AddRefs(mJARBaseURI));
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetJAREntry(&mJAREntry);
    if (NS_FAILED(rv)) return rv;

    // try to get a nsIFile directly from the url, which will often succeed.
    {
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mJARBaseURI);
        if (fileURL)
            fileURL->GetFile(getter_AddRefs(mDownloadedJARFile));
    }

    if (mDownloadedJARFile) {
        // after successfully downloading the jar file to the cache,
        // start the extraction process:
        if (mSynchronousRead)
            rv = OpenJARElement();
        else
            rv = AsyncReadJARElement();
    }
    else {
        rv = NS_NewDownloader(getter_AddRefs(mDownloader),
                              mJARBaseURI, this, nsnull, mSynchronousRead,
                              mLoadGroup, mCallbacks, mLoadFlags);

        // if DownloadComplete() was called early, need to release the reference.
        if (mSynchronousRead && mSynchronousInputStream)
            mDownloader = 0;
    }
    return rv;
}

nsresult
nsJARChannel::AsyncReadJARElement()
{
    nsresult rv;

    nsCOMPtr<nsIFileTransportService> fts = 
             do_GetService(kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsITransport> jarTransport;
    rv = fts->CreateTransportFromStreamIO(this, getter_AddRefs(jarTransport));
    if (NS_FAILED(rv)) return rv;

    if (mCallbacks) {
        nsCOMPtr<nsIProgressEventSink> sink = do_GetInterface(mCallbacks);
        if (sink) {
            // XXX don't think that this is needed anymore 
            // jarTransport->SetProgressEventSink(sink);
        }
    }

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gJarProtocolLog, PR_LOG_DEBUG)) {
        nsXPIDLCString jarURLStr;
        mURI->GetSpec(getter_Copies(jarURLStr));
        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: AsyncRead jar entry %s", (const char*)jarURLStr));
    }
#endif

    rv = jarTransport->AsyncRead(this, nsnull, 0, PRUint32(-1), 0,
                                 getter_AddRefs(mJarExtractionTransport));
    jarTransport = 0;
    return rv;
}

NS_IMETHODIMP
nsJARChannel::GetLoadFlags(PRUint32* aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetContentType(char* *aContentType)
{
    nsresult rv = NS_OK;
    if (mContentType == nsnull) {
        if (!mJAREntry)
            return NS_ERROR_FAILURE;
        char* fileName = nsCRT::strdup(mJAREntry);
        if (fileName != nsnull) {
            PRInt32 len = nsCRT::strlen(fileName);
            const char* ext = nsnull;
            for (PRInt32 i = len-1; i >= 0; i--) {
                if (fileName[i] == '.') {
                    ext = &fileName[i + 1];
                    break;
                }
            }

            if (ext) {
                nsCOMPtr<nsIMIMEService> mimeServ (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
                if (NS_SUCCEEDED(rv)) {
                    rv = mimeServ->GetTypeFromExtension(ext, &mContentType);
                }
            }
            else 
                rv = NS_ERROR_OUT_OF_MEMORY;
            
            nsCRT::free(fileName);
        } 
        else {
            rv = NS_ERROR_OUT_OF_MEMORY;
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
        rv = EnsureZipReader();
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIJAR> jar = do_QueryInterface(mJAR, &rv);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Zip reader is not an nsIJAR");
        nsCOMPtr<nsIPrincipal> certificate;
        rv = jar->GetCertificatePrincipal(mJAREntry, 
                                           getter_AddRefs(certificate));
        if (NS_FAILED(rv)) return rv;
        if (certificate)
        {   // Get the codebase principal
            nsCOMPtr<nsIScriptSecurityManager> secMan = 
                     do_GetService(kScriptSecurityManagerCID, &rv);
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
// nsIDownloadObserver methods:

NS_IMETHODIMP
nsJARChannel::OnDownloadComplete(nsIDownloader* aDownloader, nsISupports* aClosure,
                                 nsresult aStatus, nsIFile* aFile)
{
   nsresult rv=aStatus;
   if(NS_SUCCEEDED(aStatus)) {
       NS_ASSERTION(!mDownloader ||(aDownloader == mDownloader.get()), "wrong downloader");
       mDownloadedJARFile = aFile;
       // after successfully downloading the jar file to the cache,
       // start the extraction process:
       if (mSynchronousRead)
           rv = OpenJARElement();
       else
           rv = AsyncReadJARElement();
   }
   mDownloader = 0;
   return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver methods:

NS_IMETHODIMP
nsJARChannel::OnStartRequest(nsIRequest* jarExtractionTransport,
                             nsISupports* context)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(), "wrong thread");
#endif
    return mUserListener->OnStartRequest(this, mUserContext);
}

NS_IMETHODIMP
nsJARChannel::OnStopRequest(nsIRequest* jarExtractionTransport, nsISupports* context, 
                            nsresult aStatus)
{
    nsresult rv;
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(), "wrong thread");
#endif
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gJarProtocolLog, PR_LOG_DEBUG)) {
        nsCOMPtr<nsIURI> jarURI;
        nsXPIDLCString jarURLStr;
        rv = mURI->GetSpec(getter_Copies(jarURLStr));
        if (NS_SUCCEEDED(rv)) {
            PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
                   ("nsJarProtocol: jar extraction complete %s status=%x",
                    (const char*)jarURLStr, aStatus));
        }
    }
#endif

    rv = mUserListener->OnStopRequest(this, mUserContext, aStatus);
    NS_ASSERTION(NS_SUCCEEDED(rv), "OnStopRequest failed");

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, context, aStatus);

    mUserListener = nsnull;
    mUserContext = nsnull;
    mJarExtractionTransport = nsnull;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsJARChannel::OnDataAvailable(nsIRequest* jarCacheTransport, 
                              nsISupports* context, 
                              nsIInputStream *inStr, 
                              PRUint32 sourceOffset, 
                              PRUint32 count)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(), "wrong thread");
#endif
    return mUserListener->OnDataAvailable(this, mUserContext, 
                                          inStr, sourceOffset, count);
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamIO methods:

nsresult
nsJARChannel::EnsureZipReader()
{
    if (mJAR == nsnull) {
        nsresult rv;
        if (mDownloadedJARFile == nsnull)
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsIZipReaderCache> jarCache;
        rv = mJARProtocolHandler->GetJARCache(getter_AddRefs(jarCache));
        if (NS_FAILED(rv)) return rv; 

        rv = jarCache->GetZip(mDownloadedJARFile, getter_AddRefs(mJAR));
        if (NS_FAILED(rv)) return rv; 
    }
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::Open(char* *contentType, PRInt32 *contentLength) 
{
    nsresult rv;
    rv = EnsureZipReader();
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
    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::GetInputStream(nsIInputStream* *aInputStream) 
{
#ifdef PR_LOGGING
    if (PR_LOG_TEST(gJarProtocolLog, PR_LOG_DEBUG)) {
        nsXPIDLCString jarURLStr;
        mURI->GetSpec(getter_Copies(jarURLStr));
        PR_LOG(gJarProtocolLog, PR_LOG_DEBUG,
               ("nsJarProtocol: GetInputStream jar entry %s", (const char*)jarURLStr));
    }
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
