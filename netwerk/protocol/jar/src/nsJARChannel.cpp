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
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kZipReaderCID, NS_ZIPREADER_CID);
static NS_DEFINE_CID(kFileChannelCID, NS_FILECHANNEL_CID);

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

        if (NS_SUCCEEDED(status) && mJARChannel->mJarCacheTransport) {
            NS_ASSERTION(jarCacheTransport == (mJARChannel->mJarCacheTransport).get(),
                         "wrong transport");
            // after successfully downloading the jar file to the cache,
            // start the extraction process:
            nsCOMPtr<nsIFileChannel> jarCacheFile;
            rv = NS_NewFileChannel(mJarCacheFile, 
                                   PR_RDONLY,
                                   nsnull,      // XXX content type
                                   0,           // XXX content length
                                   mJARChannel->mLoadGroup,
                                   mJARChannel->mCallbacks,
                                   mJARChannel->mLoadAttributes,
                                   nsnull,
                                   mJARChannel->mBufferSegmentSize,
                                   mJARChannel->mBufferMaxSize,
                                   getter_AddRefs(jarCacheFile));
            if (NS_FAILED(rv)) return rv;

            rv = mJARChannel->ExtractJARElement(jarCacheFile);
        }
        mJARChannel->mJarCacheTransport = nsnull;
        return rv;
    }

    nsJARDownloadObserver(nsIFile* jarCacheFile, nsJARChannel* jarChannel) {
        NS_INIT_REFCNT();
        mJarCacheFile = jarCacheFile;
        mJARChannel = jarChannel;
        NS_ADDREF(mJARChannel);
    }

    virtual ~nsJARDownloadObserver() {
        NS_RELEASE(mJARChannel);
    }

protected:
    nsCOMPtr<nsIFile>   mJarCacheFile;
    nsJARChannel*       mJARChannel;
};

NS_IMPL_ISUPPORTS1(nsJARDownloadObserver, nsIStreamObserver)

////////////////////////////////////////////////////////////////////////////////

nsJARChannel::nsJARChannel()
    : mCommand(nsnull),
      mContentType(nsnull),
      mJAREntry(nsnull),
      mMonitor(nsnull)
{
    NS_INIT_REFCNT();
}

nsJARChannel::~nsJARChannel()
{
    if (mCommand)
        nsCRT::free(mCommand);
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
nsJARChannel::Init(nsIJARProtocolHandler* aHandler, 
                   const char* command, 
                   nsIURI* uri,
                   nsILoadGroup* aLoadGroup, 
                   nsIInterfaceRequestor* notificationCallbacks,
                   nsLoadFlags loadAttributes,
                   nsIURI* originalURI,
                   PRUint32 bufferSegmentSize,
                   PRUint32 bufferMaxSize)
{
    nsresult rv;
	mURI = do_QueryInterface(uri, &rv);
    if (NS_FAILED(rv)) return rv;
	mCommand = nsCRT::strdup(command);
    if (mCommand == nsnull)
        return NS_ERROR_OUT_OF_MEMORY; 
    mOriginalURI = originalURI ? originalURI : uri;
    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
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
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::Cancel()
{
    nsresult rv;
    nsAutoMonitor monitor(mMonitor);

    if (mJarCacheTransport) {
        rv = mJarCacheTransport->Cancel();
        if (NS_FAILED(rv)) return rv;
        mJarCacheTransport = nsnull;
    }
    if (mJarExtractionTransport) {
        rv = mJarExtractionTransport->Cancel();
        if (NS_FAILED(rv)) return rv;
        mJarExtractionTransport = nsnull;
    }

	return NS_OK;
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
    *aOriginalURI = mOriginalURI;
    NS_ADDREF(*aOriginalURI);
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
nsJARChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount, 
							  nsIInputStream* *result)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream* *result)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::AsyncOpen(nsIStreamObserver* observer, nsISupports* ctxt)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount, 
						nsISupports* ctxt, 
						nsIStreamListener* listener)
{
    nsresult rv;
    rv = mURI->GetJARFile(getter_AddRefs(mJARBaseURI));
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetJAREntry(&mJAREntry);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> jarBaseChannel;
    rv = NS_OpenURI(getter_AddRefs(jarBaseChannel),
                    mJARBaseURI, mLoadGroup, mCallbacks, mLoadAttributes);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFileChannel> jarBaseFile = do_QueryInterface(jarBaseChannel, &rv);

    // XXX need to set a state variable here to say we're reading
    mStartPosition = startPosition;
    mReadCount = readCount;
    mUserContext = ctxt;
    mUserListener = listener;

    if (NS_SUCCEEDED(rv)) {
        // then we've already got a local jar file -- no need to download it
        rv = ExtractJARElement(jarBaseFile);
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
            nsCOMPtr<nsIFileChannel> fileChannel;
            rv = nsComponentManager::CreateInstance(kFileChannelCID,
                                                    nsnull,
                                                    NS_GET_IID(nsIFileChannel),
                                                    getter_AddRefs(fileChannel));
            if (NS_FAILED(rv)) return rv;

            rv = fileChannel->Init(jarCacheFile, 
                                   PR_RDONLY,
                                   nsnull,       // contentType
                                   -1,           // contentLength
                                   nsnull,       // loadGroup
                                   nsnull,       // notificationCallbacks
                                   nsIChannel::LOAD_NORMAL,
                                   nsnull,       // originalURI
                                   mBufferSegmentSize,
                                   mBufferMaxSize);
            if (NS_FAILED(rv)) return rv;

		    rv = ExtractJARElement(fileChannel);
			return rv;
		}

        NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsAutoMonitor monitor(mMonitor);

        // use a file transport to serve as a data pump for the download (done
        // on some other thread)
        nsCOMPtr<nsIChannel> jarCacheTransport;
        rv = fts->CreateTransport(jarCacheFile, PR_RDONLY, mCommand,
                                  mBufferSegmentSize, mBufferMaxSize,
                                  getter_AddRefs(mJarCacheTransport));
        if (NS_FAILED(rv)) return rv;

        rv = mJarCacheTransport->SetNotificationCallbacks(mCallbacks);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIStreamObserver> downloadObserver = new nsJARDownloadObserver(jarCacheFile, this);
        if (downloadObserver == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCOMPtr<nsIInputStream> jarBaseIn;
        rv = jarBaseChannel->OpenInputStream(0, -1, getter_AddRefs(jarBaseIn));
        if (NS_FAILED(rv)) return rv;

        rv = mJarCacheTransport->AsyncWrite(jarBaseIn, 0, -1, nsnull, downloadObserver);
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
nsJARChannel::ExtractJARElement(nsIFileChannel* jarBaseFile)
{
    nsresult rv;

    nsAutoMonitor monitor(mMonitor);

    mJARBaseFile = jarBaseFile;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransportFromFileSystem(this, mCommand,
                                            mBufferSegmentSize, mBufferMaxSize,
                                            getter_AddRefs(mJarExtractionTransport));
    if (NS_FAILED(rv)) return rv;

    rv = mJarExtractionTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) return rv;

    rv = mJarExtractionTransport->AsyncRead(mStartPosition, mReadCount, nsnull, this);
    return rv;
}

NS_IMETHODIMP
nsJARChannel::AsyncWrite(nsIInputStream* fromStream, PRUint32 startPosition, 
						 PRInt32 writeCount, 
						 nsISupports* ctxt, 
						 nsIStreamObserver* observer)
{
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
    nsCOMPtr<nsIPrincipal> principal;
    nsresult rv = mJAR->GetPrincipal(mJAREntry, getter_AddRefs(principal));
    if (NS_SUCCEEDED(rv) && principal)
        rv = principal->QueryInterface(NS_GET_IID(nsISupports), (void **)aOwner);
    else
        *aOwner = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsJARChannel::SetOwner(nsISupports* aOwner)
{
    //XXX: is this OK?
    return NS_ERROR_FAILURE;
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
    rv = mUserListener->OnStopRequest(this, mUserContext, status, aMsg);
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

    rv = entry->GetRealSize((PRUint32*)contentLength);
    if (NS_FAILED(rv)) return rv;

    return GetContentType(contentType);
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
	return mJAR->GetInputStream(mJAREntry, aInputStream);
}
 
NS_IMETHODIMP
nsJARChannel::GetOutputStream(nsIOutputStream* *aOutputStream) 
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIJARChannel methods:

NS_IMETHODIMP
nsJARChannel::EnumerateEntries(const char *aRoot, nsISimpleEnumerator **_retval)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
