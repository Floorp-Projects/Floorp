/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFileChannel.h"
#include "nsIURL.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "netCore.h"
#include "nsIFileTransportService.h"
#include "nsIFile.h"
#include "nsInt64.h"
#include "nsMimeTypes.h"
#include "prio.h"	// Need to pick up def of PR_RDONLY

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mIOFlags(-1),
      mPerm(-1),
      mLoadAttributes(LOAD_NORMAL),
      mTransferOffset(0),
      mTransferCount(-1),
      mBufferSegmentSize(0),
      mBufferMaxSize(0),
      mStatus(NS_OK)
#ifdef DEBUG
      ,mInitiator(nsnull)
#endif
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(PRInt32 ioFlags,
                    PRInt32 perm,
                    nsIURI* uri)
{
    nsresult rv;

    mIOFlags = ioFlags;
    mPerm = perm;
    mURI = uri;

    // if we support the nsIURL interface then use it to get just
    // the file path with no other garbage!
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mURI, &rv);
    if (NS_FAILED(rv)) {
        // this URL doesn't denote a file
        return NS_ERROR_MALFORMED_URI;
    }

    rv = fileURL->GetFile(getter_AddRefs(mFile));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(mFile);
    if (localFile)
        localFile->SetFollowLinks(PR_TRUE);

    return rv;
}

nsFileChannel::~nsFileChannel()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS7(nsFileChannel,
                   nsIFileChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamListener,
                   nsIStreamObserver,
                   nsIProgressEventSink,
                   nsIInterfaceRequestor)

NS_METHOD
nsFileChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
  
    nsFileChannel* fc = new nsFileChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetName(PRUnichar* *result)
{
    if (mFileTransport)
        return mFileTransport->GetName(result);
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
nsFileChannel::IsPending(PRBool *result)
{
    if (mFileTransport)
        return mFileTransport->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    mStatus = status;
    if (mFileTransport)
        return mFileTransport->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (mFileTransport)
        return mFileTransport->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (mFileTransport)
        return mFileTransport->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetURI(nsIURI* aURI)
{
    mURI = aURI;
    return NS_OK;
}

nsresult
nsFileChannel::EnsureTransport()
{
    nsresult rv;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mFile, mIOFlags, mPerm, 
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) return rv;

    if (mLoadAttributes != LOAD_NORMAL) {
        rv = mFileTransport->SetLoadAttributes(mLoadAttributes);
        if (NS_FAILED(rv)) return rv;
    }
    if (mBufferSegmentSize) {
        rv = mFileTransport->SetBufferSegmentSize(mBufferSegmentSize);
        if (NS_FAILED(rv)) return rv;
    }
    if (mBufferMaxSize) {
        rv = mFileTransport->SetBufferMaxSize(mBufferMaxSize);
        if (NS_FAILED(rv)) return rv;
    }
    if (mCallbacks) {
        rv = mFileTransport->SetNotificationCallbacks(this);
        if (NS_FAILED(rv)) return rv;
    }
    if (mTransferOffset) {
        rv = mFileTransport->SetTransferOffset(mTransferOffset);
        if (NS_FAILED(rv)) return rv;
    }
    if (mTransferCount >= 0) {
        rv = mFileTransport->SetTransferCount(mTransferCount);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OpenInputStream(nsIInputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    rv = EnsureTransport();
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->OpenInputStream(result);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(nsIOutputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    mIOFlags |= PR_WRONLY;

    rv = EnsureTransport();
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->OpenOutputStream(result);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(nsIStreamListener *listener,
                         nsISupports *ctxt)
{
    nsresult rv;

#ifdef DEBUG
    NS_ASSERTION(mInitiator == nsnull || mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
    mInitiator = PR_CurrentThread();
#endif

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    mRealListener = listener;
    nsCOMPtr<nsIStreamListener> tempListener = this;

    if (mLoadGroup) {
        nsCOMPtr<nsILoadGroupListenerFactory> factory;
        //
        // Create a load group "proxy" listener...
        //
        rv = mLoadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
        if (factory) {
            nsIStreamListener *newListener;
            rv = factory->CreateLoadGroupListener(mRealListener, &newListener);
            if (NS_SUCCEEDED(rv)) {
                mRealListener = newListener;
                NS_RELEASE(newListener);
            }
        }

        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    rv = EnsureTransport();
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->AsyncRead(tempListener, ctxt);

  done:
    if (NS_FAILED(rv)) {
        nsresult rv2 = mLoadGroup->RemoveChannel(this, ctxt, rv, nsnull);       // XXX fix error message
        NS_ASSERTION(NS_SUCCEEDED(rv2), "RemoveChannel failed");
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          nsIStreamObserver *observer,
                          nsISupports *ctxt)
{
    nsresult rv;

#ifdef DEBUG
    NS_ASSERTION(mInitiator == nsnull || mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
    mInitiator = PR_CurrentThread();
#endif

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    mIOFlags |= PR_WRONLY;

    if (mLoadGroup) {
        nsCOMPtr<nsILoadGroupListenerFactory> factory;
        //
        // Create a load group "proxy" listener...
        //
        rv = mLoadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
        if (factory) {
            nsIStreamListener *newListener;
            rv = factory->CreateLoadGroupListener(mRealListener, &newListener);
            if (NS_SUCCEEDED(rv)) {
                mRealListener = newListener;
                NS_RELEASE(newListener);
            }
        }

        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    rv = EnsureTransport();
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->AsyncWrite(fromStream, observer, ctxt);

  done:
    if (NS_FAILED(rv)) {
        nsresult rv2 = mLoadGroup->RemoveChannel(this, ctxt, rv, nsnull);       // XXX fix error message
        NS_ASSERTION(NS_SUCCEEDED(rv2), "RemoveChannel failed");
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentType(char * *aContentType)
{
    nsresult rv = NS_OK;

    *aContentType = nsnull;
    if (mContentType.IsEmpty()) {
        PRBool directory;
		mFile->IsDirectory(&directory);
		if (directory) {
            mContentType = "application/http-index-format";
        }
        else {
            nsCOMPtr<nsIMIMEService> MIMEService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
            if (NS_FAILED(rv)) return rv;

            rv = MIMEService->GetTypeFromFile(mFile, aContentType);
            if (NS_SUCCEEDED(rv)) {
                mContentType = *aContentType;
                return rv;
            }
        }

        if (mContentType.IsEmpty()) {
            mContentType = UNKNOWN_CONTENT_TYPE;
        }
    }
    *aContentType = mContentType.ToNewCString();

    if (!*aContentType) {
        return NS_ERROR_OUT_OF_MEMORY;
    } else {
        return NS_OK;
    }
}

NS_IMETHODIMP
nsFileChannel::SetContentType(const char *aContentType)
{
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentLength(PRInt32 *aContentLength)
{
    nsresult rv;
    PRInt64 size;
    rv = mFile->GetFileSize(&size);
    if (NS_SUCCEEDED(rv)) {
        *aContentLength = nsInt64(size);
    } else {
        *aContentLength = -1;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("nsFileChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    *aTransferOffset = mTransferOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    mTransferOffset = aTransferOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    *aTransferCount = mTransferCount;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetTransferCount(PRInt32 aTransferCount)
{
    mTransferCount = aTransferCount;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    *aBufferSegmentSize = mBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    mBufferSegmentSize = aBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    *aBufferMaxSize = mBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    mBufferMaxSize = aBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetLocalFile(nsIFile* *file)
{
    *file = mFile;
    NS_ADDREF(*file);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsFileChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;

  // Cache the new nsIProgressEventSink instance...
  if (mCallbacks) {
    (void)mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink),
                                   getter_AddRefs(mProgress));
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsFileChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnStartRequest(nsIChannel* transportChannel, nsISupports* context)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif
    NS_ASSERTION(mRealListener, "No listener...");
    return mRealListener->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsFileChannel::OnStopRequest(nsIChannel* transportChannel, nsISupports* context,
                             nsresult aStatus, const PRUnichar* aStatusArg)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif

    nsresult rv;

    rv = mRealListener->OnStopRequest(this, context, aStatus, aStatusArg);

    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, aStatus, aStatusArg);
        }
    }

    // Release the reference to the consumer stream listener...
    mRealListener = null_nsCOMPtr();
    mFileTransport = null_nsCOMPtr();
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OnDataAvailable(nsIChannel* transportChannel, nsISupports* context,
                               nsIInputStream *aIStream, PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif

    nsresult rv;
    rv = mRealListener->OnDataAvailable(this, context, aIStream,
                                        aSourceOffset, aLength);

    //
    // If the connection is being aborted cancel the transport.  This will
    // insure that the transport will go away even if it is blocked waiting
    // for the consumer to empty the pipe...
    //
    if (NS_FAILED(rv)) {
        mFileTransport->Cancel(rv);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIInterfaceRequestor
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetInterface(const nsIID &anIID, void **aResult )
{
    // capture the progress event sink stuff. pass the rest through.
    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
        *aResult = NS_STATIC_CAST(nsIProgressEventSink*, this);
        NS_ADDREF(this);
        return NS_OK;
    } 
    else if (mCallbacks) {
        return mCallbacks->GetInterface(anIID, aResult);
    }
    return NS_ERROR_NO_INTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIProgressEventSink
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnStatus(nsIChannel *aChannel, nsISupports* ctxt, 
                        nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv = NS_OK;
    if (mProgress) {
        rv = mProgress->OnStatus(this, ctxt, aStatus, aStatusArg);
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OnProgress(nsIChannel* aChannel,
                          nsISupports* aContext,
                          PRUint32 aProgress,
                          PRUint32 aProgressMax)
{
    nsresult rv;
    if (mProgress) {
        rv = mProgress->OnProgress(this, aContext, aProgress, aProgressMax);
        NS_ASSERTION(NS_SUCCEEDED(rv), "dropping error result");
    }
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// From nsIFileChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::Init(nsIFile* file, 
                    PRInt32 ioFlags,
                    PRInt32 perm)
{
    nsresult rv;
    nsCOMPtr<nsIFileURL> url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIFileURL),
                                            getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    rv = url->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    return Init(ioFlags, perm, url);
}

NS_IMETHODIMP
nsFileChannel::GetFile(nsIFile* *result)
{
    *result = mFile;
    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetIoFlags(PRInt32 *aIoFlags)
{
    *aIoFlags = mIOFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetIoFlags(PRInt32 aIoFlags)
{
    mIOFlags = aIoFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetPermissions(PRInt32 *aPermissions)
{
    *aPermissions = mPerm;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetPermissions(PRInt32 aPermissions)
{
    mPerm = aPermissions;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
