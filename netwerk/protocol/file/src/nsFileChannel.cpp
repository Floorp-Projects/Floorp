/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFileChannel.h"
#include "nsDirectoryIndexStream.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsEventQueueUtils.h"

#include "nsIServiceManager.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamTransportService.h"
#include "nsITransport.h"
#include "nsIFileURL.h"
#include "nsIMIMEService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------

nsFileChannel::nsFileChannel()
    : mContentLength(-1)
    , mUploadLength(-1)
    , mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
    , mIsDir(PR_FALSE)
    , mUploading(PR_FALSE)
{
}

nsresult
nsFileChannel::Init(nsIURI *uri)
{
    nsresult rv;
    mURL = do_QueryInterface(uri, &rv);
    return rv;
}

nsresult
nsFileChannel::GetClonedFile(nsIFile **result)
{
    nsresult rv;

    nsCOMPtr<nsIFile> file;
    rv = mURL->GetFile(getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    return file->Clone(result);
}

nsresult
nsFileChannel::EnsureStream()
{
    NS_ENSURE_TRUE(mURL, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    nsCOMPtr<nsIFile> file;

    // don't assume nsIFile impl is threadsafe; pass a clone to the stream.
    rv = GetClonedFile(getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    // we accept that this might result in a disk hit to stat the file
    rv = file->IsDirectory(&mIsDir);
    if (NS_FAILED(rv)) {
        // canonicalize error message
        if (rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
            rv = NS_ERROR_FILE_NOT_FOUND;
        return rv;
    }

    if (mIsDir)
        rv = nsDirectoryIndexStream::Create(file, getter_AddRefs(mStream));
    else
        rv = NS_NewLocalFileInputStream(getter_AddRefs(mStream), file);

    if (NS_FAILED(rv)) return rv;

    // fixup content length
    if (mStream && (mContentLength < 0))
        mStream->Available((PRUint32 *) &mContentLength);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsISupports
//-----------------------------------------------------------------------------

// XXX this only needs to be threadsafe because of bug 101252
NS_IMPL_THREADSAFE_ISUPPORTS7(nsFileChannel,
                              nsIRequest,
                              nsIChannel,
                              nsIStreamListener,
                              nsIRequestObserver,
                              nsIUploadChannel,
                              nsIFileChannel,
                              nsITransportEventSink)

//-----------------------------------------------------------------------------
// nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFileChannel::GetName(nsACString &result)
{
    return mURL->GetSpec(result);
}

NS_IMETHODIMP
nsFileChannel::IsPending(PRBool *result)
{
    *result = (mRequest != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetStatus(nsresult *status)
{
    if (NS_SUCCEEDED(mStatus) && mRequest)
        mRequest->GetStatus(status);
    else
        *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Cancel(nsresult status)
{
    NS_ENSURE_TRUE(mRequest, NS_ERROR_UNEXPECTED);
    mStatus = status;
    return mRequest->Cancel(status);
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
    NS_ENSURE_TRUE(mRequest, NS_ERROR_UNEXPECTED);
    return mRequest->Suspend();
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    NS_ENSURE_TRUE(mRequest, NS_ERROR_UNEXPECTED);
    return mRequest->Resume();
}

NS_IMETHODIMP
nsFileChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFileChannel::GetOriginalURI(nsIURI **aURI)
{
    if (mOriginalURI)
        *aURI = mOriginalURI;
    else
        *aURI = mURL;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetOriginalURI(nsIURI *aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI **aURI)
{
    NS_IF_ADDREF(*aURI = mURL);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetOwner(nsISupports **aOwner)
{
    NS_IF_ADDREF(*aOwner = mOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetOwner(nsISupports *aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
    NS_IF_ADDREF(*aCallbacks = mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
    mCallbacks = aCallbacks;
    mProgressSink = do_GetInterface(mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP 
nsFileChannel::GetSecurityInfo(nsISupports **aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentType(nsACString &aContentType)
{
    NS_PRECONDITION(mURL, "Why is this being called?");
    
    if (mContentType.IsEmpty()) {
        if (mIsDir) {
            mContentType.AssignLiteral(APPLICATION_HTTP_INDEX_FORMAT);
        } else {
            // Get content type from file extension
            nsCOMPtr<nsIFile> file;
            nsresult rv = mURL->GetFile(getter_AddRefs(file));
            if (NS_FAILED(rv)) return rv;
            
            nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
            if (NS_SUCCEEDED(rv))
                mime->GetTypeFromFile(file, mContentType);

            if (mContentType.IsEmpty())
                mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
        }
    }
    
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetContentType(const nsACString &aContentType)
{
    // If someone gives us a type hint we should just use that type instead of
    // doing our guessing.  So we don't care when this is being called.

    // mContentCharset is unchanged if not parsed
    NS_ParseContentType(aContentType, mContentType, mContentCharset);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetContentCharset(const nsACString &aContentCharset)
{
    // If someone gives us a charset hint we should just use that charset.
    // So we don't care when this is being called.
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetContentLength(PRInt32 aContentLength)
{
    // XXX does this really make any sense at all?
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Open(nsIInputStream **result)
{
    NS_ENSURE_TRUE(!mRequest, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_TRUE(!mUploading, NS_ERROR_NOT_IMPLEMENTED); // XXX implement me!

    nsresult rv;

    rv = EnsureStream();
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = mStream);

    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctx)
{
    NS_ENSURE_TRUE(!mRequest, NS_ERROR_IN_PROGRESS);

    nsCOMPtr<nsIStreamListener> grip;
    nsCOMPtr<nsIEventQueue> currentEventQ;
    nsresult rv;

    rv = NS_GetCurrentEventQ(getter_AddRefs(currentEventQ));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamTransportService> sts =
            do_GetService(kStreamTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (mUploading) {
        //
        // open file output stream.  since output stream will be accessed on a
        // background thread, we should not give it a reference to "our" nsIFile
        // instance.
        //
        nsCOMPtr<nsIFile> file;
        rv = GetClonedFile(getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIOutputStream> fileOut;
        rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOut), file,
                                         PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                                         PR_IRUSR | PR_IWUSR);
        if (NS_FAILED(rv)) return rv;

        //
        // create asynchronous output stream wrapping file output stream.
        //
        nsCOMPtr<nsITransport> transport;
        rv = sts->CreateOutputTransport(fileOut, -1, mUploadLength, PR_TRUE,
                                        getter_AddRefs(transport));
        if (NS_FAILED(rv)) return rv;

        rv = transport->SetEventSink(this, currentEventQ);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIOutputStream> asyncOut;
        rv = transport->OpenOutputStream(0, 0, 0, getter_AddRefs(asyncOut));
        if (NS_FAILED(rv)) return rv;

        //
        // create async stream copier
        //
        // XXX copier might read more than mUploadLength from mStream!!  not
        // a huge deal, but probably should be fixed.
        //
        nsCOMPtr<nsIAsyncStreamCopier> copier;
        rv = NS_NewAsyncStreamCopier(getter_AddRefs(copier), mStream, asyncOut,
                                     nsnull,   // perform copy using default i/o thread
                                     PR_FALSE, // assume the upload stream is unbuffered
                                     PR_TRUE); // but, the async output stream is buffered!
        if (NS_FAILED(rv)) return rv;

        rv = copier->AsyncCopy(this, nsnull);
        if (NS_FAILED(rv)) return rv;

        mRequest = copier;
    }
    else {
        //
        // create file input stream
        //
        rv = EnsureStream();
        if (NS_FAILED(rv)) return rv;

        //
        // create asynchronous input stream wrapping file input stream.
        //
        nsCOMPtr<nsITransport> transport;
        rv = sts->CreateInputTransport(mStream, -1, -1, PR_TRUE,
                                       getter_AddRefs(transport));
        if (NS_FAILED(rv)) return rv;

        rv = transport->SetEventSink(this, currentEventQ);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIInputStream> asyncIn;
        rv = transport->OpenInputStream(0, 0, 0, getter_AddRefs(asyncIn));
        if (NS_FAILED(rv)) return rv;

        //
        // create input stream pump
        //
        nsCOMPtr<nsIInputStreamPump> pump;
        rv = NS_NewInputStreamPump(getter_AddRefs(pump), asyncIn);
        if (NS_FAILED(rv)) return rv;

        rv = pump->AsyncRead(this, nsnull);
        if (NS_FAILED(rv)) return rv;

        mRequest = pump;
    }

    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    mListener = listener;
    mListenerContext = ctx;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIFileChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFileChannel::GetFile(nsIFile **file)
{
    return mURL->GetFile(file);
}

//-----------------------------------------------------------------------------
// nsIUploadChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFileChannel::SetUploadStream(nsIInputStream *stream,
                               const nsACString &contentType,
                               PRInt32 contentLength)
{
    NS_ENSURE_TRUE(!mRequest, NS_ERROR_IN_PROGRESS);

    mStream = stream;

    if (mStream) {
        mUploading = PR_TRUE;
        mUploadLength = contentLength;

        if (mUploadLength < 0) {
            // make sure we know how much data we are uploading.
            nsresult rv = mStream->Available((PRUint32 *) &mUploadLength);
            if (NS_FAILED(rv)) return rv;
        }
    }
    else {
        mUploading = PR_FALSE;
        mUploadLength = -1;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetUploadStream(nsIInputStream **stream)
{
    if (mUploading)
        NS_IF_ADDREF(*stream = mStream);
    else
        *stream = nsnull;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFileChannel::OnStartRequest(nsIRequest *req, nsISupports *ctx)
{
    return mListener->OnStartRequest(this, mListenerContext);
}

NS_IMETHODIMP
nsFileChannel::OnStopRequest(nsIRequest *req, nsISupports *ctx, nsresult status)
{
    if (NS_SUCCEEDED(mStatus))
        mStatus = status;

    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    mRequest = 0;
    mStream = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OnDataAvailable(nsIRequest *req, nsISupports *ctx,
                               nsIInputStream *stream,
                               PRUint32 offset, PRUint32 count)
{
    return mListener->OnDataAvailable(this, mListenerContext, stream, offset, count);
}

//-----------------------------------------------------------------------------
// nsITransportEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFileChannel::OnTransportStatus(nsITransport *trans, nsresult status,
                                 PRUint32 progress, PRUint32 progressMax)
{
    // suppress status notification if channel is no longer pending!
    if (mProgressSink && NS_SUCCEEDED(mStatus) && mRequest && !(mLoadFlags & LOAD_BACKGROUND)) {
        // file channel does not send OnStatus events!
        if (status == nsITransport::STATUS_READING ||
            status == nsITransport::STATUS_WRITING) {
            mProgressSink->OnProgress(this, nsnull, progress, progressMax);
        }
    }
    return NS_OK;
}


