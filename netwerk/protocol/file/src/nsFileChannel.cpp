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
#include "nsIMIMEService.h"
#include "netCore.h"
#include "nsIFileTransportService.h"
#include "nsIFile.h"
#include "nsInt64.h"
#include "nsMimeTypes.h"
#include "prio.h"	// Need to pick up def of PR_RDONLY

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mLoadAttributes(LOAD_NORMAL)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(const char* command,
                    nsIURI* uri,
                    nsILoadGroup* aLoadGroup,
                    nsIInterfaceRequestor* notificationCallbacks,
                    nsLoadFlags loadAttributes,
                    nsIURI* originalURI,
                    PRUint32 bufferSegmentSize, 
                    PRUint32 bufferMaxSize)
{
    nsresult rv;

    mOriginalURI = originalURI ? originalURI : uri;
    mURI = uri;
    mCommand = nsCRT::strdup(command);
    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;
    if (mCommand == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    // if we support the nsIURL interface then use it to get just
    // the file path with no other garbage!
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mURI, &rv);
    if (NS_FAILED(rv)) {
        // this URL doesn't denote a file
        return NS_ERROR_MALFORMED_URI;
    }

    rv = fileURL->GetFile(getter_AddRefs(mFile));
    if (NS_FAILED(rv)) return rv;

    return rv;
}

nsFileChannel::~nsFileChannel()
{
    if (mCommand) nsCRT::free(mCommand);
}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsFileChannel,
                   nsIFileChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamListener,
                   nsIStreamObserver)

NS_METHOD
nsFileChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
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
nsFileChannel::IsPending(PRBool *result)
{
    if (mFileTransport)
        return mFileTransport->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Cancel()
{
    if (mFileTransport)
        return mFileTransport->Cancel();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
    if (mFileTransport)
        return mFileTransport->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    if (mFileTransport)
        return mFileTransport->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetOriginalURI(nsIURI * *aURI)
{
    *aURI = mOriginalURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI * *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mFile, PR_RDONLY, mCommand, 0, 0, getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->OpenInputStream(startPosition, readCount, result);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mFile, PR_RDONLY, mCommand, mBufferSegmentSize, mBufferMaxSize,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->OpenOutputStream(startPosition, result);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIStreamListener *listener)
{
    nsresult rv;

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

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mFile, PR_RDONLY, mCommand, mBufferSegmentSize, mBufferMaxSize,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->AsyncRead(startPosition, readCount, ctxt, tempListener);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mFile, PR_RDONLY, mCommand, mBufferSegmentSize, mBufferMaxSize,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->AsyncWrite(fromStream, startPosition, writeCount, ctxt, observer);
  done:
    if (NS_FAILED(rv)) {
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
            NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
            if (NS_FAILED(rv)) return rv;

            rv = MIMEService->GetTypeFromURI(mURI, aContentType);
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
    NS_ASSERTION(mRealListener, "No listener...");
    return mRealListener->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsFileChannel::OnStopRequest(nsIChannel* transportChannel, nsISupports* context,
                             nsresult aStatus, const PRUnichar* aMsg)
{
    nsresult rv;

    rv = mRealListener->OnStopRequest(this, context, aStatus, aMsg);

    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, aStatus, aMsg);
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
    nsresult rv;

    rv = mRealListener->OnDataAvailable(this, context, aIStream,
                                        aSourceOffset, aLength);

    //
    // If the connection is being aborted cancel the transport.  This will
    // insure that the transport will go away even if it is blocked waiting
    // for the consumer to empty the pipe...
    //
    if (NS_FAILED(rv)) {
        mFileTransport->Cancel();
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIFileChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::Init(nsIFile* file, 
                    PRInt32 mode,
                    const char* contentType, 
                    PRInt32 contentLength, 
                    nsILoadGroup* aLoadGroup,
                    nsIInterfaceRequestor* notificationCallbacks,
                    nsLoadFlags loadAttributes, 
                    nsIURI* originalURI,
                    PRUint32 bufferSegmentSize, 
                    PRUint32 bufferMaxSize)
{
    nsresult rv;
    nsCOMPtr<nsIFileURL> url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIFileURL),
                                            getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    rv = url->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    return Init("load", // XXX 
                url,
                aLoadGroup,
                notificationCallbacks,
                loadAttributes, 
                originalURI,
                bufferSegmentSize, 
                bufferMaxSize);
}

NS_IMETHODIMP
nsFileChannel::GetFile(nsIFile* *result)
{
    *result = mFile;
    NS_ADDREF(*result);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////







