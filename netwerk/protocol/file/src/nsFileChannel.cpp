/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFileChannel.h"
#include "nsIFileChannel.h"
#include "nsIStreamConverterService.h"
#include "nsIFileURL.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsIFileTransportService.h"
#include "nsIFile.h"
#include "nsIEventQueueService.h"
#include "nsIOutputStream.h"
#include "nsXPIDLString.h"
#include "nsStandardURL.h"
#include "nsReadableUtils.h"
#include "nsInt64.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "prio.h"   // Need to pick up def of PR_RDONLY

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mIOFlags(-1),
      mPerm(-1),
      mLoadFlags(LOAD_NORMAL),
      mTransferOffset(0),
      mTransferCount(-1),
      mBufferSegmentSize(0),
      mBufferMaxSize(0),
      mStatus(NS_OK)
#ifdef DEBUG
      ,mInitiator(nsnull)
#endif
{
    NS_INIT_ISUPPORTS();
}

nsresult
nsFileChannel::Init(PRInt32 ioFlags,
                    PRInt32 perm,
                    nsIURI* uri,
                    PRBool generateHTMLDirs)
{
    mIOFlags = ioFlags;
    mPerm = perm;
    mURI = uri;
    mGenerateHTMLDirs = generateHTMLDirs;
    return NS_OK;
}

nsFileChannel::~nsFileChannel()
{
}

nsresult
nsFileChannel::SetStreamConverter()
{
    nsresult rv;
    nsCOMPtr<nsIStreamListener> converterListener = mRealListener;

    nsCOMPtr<nsIStreamConverterService> scs = do_GetService(kStreamConverterServiceCID, &rv);

    if (NS_FAILED(rv))
        return rv;

    rv = scs->AsyncConvertData(NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                               NS_LITERAL_STRING(TEXT_HTML).get(),
                               converterListener,
                               mURI,
                               getter_AddRefs(mRealListener));

    return rv;
}

NS_IMPL_THREADSAFE_ADDREF(nsFileChannel)
NS_IMPL_THREADSAFE_RELEASE(nsFileChannel)
NS_INTERFACE_MAP_BEGIN(nsFileChannel)
    NS_INTERFACE_MAP_ENTRY(nsIFileChannel)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIStreamProvider)
    NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequestObserver, nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFileChannel)
NS_INTERFACE_MAP_END

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
nsFileChannel::GetName(nsACString &result)
{
    if (mCurrentRequest)
        return mCurrentRequest->GetName(result);
    return mURI->GetSpec(result);
}

NS_IMETHODIMP
nsFileChannel::IsPending(PRBool *result)
{
    if (mCurrentRequest)
        return mCurrentRequest->IsPending(result);
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
    NS_ASSERTION(mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
#endif
    mStatus = status;
    if (mCurrentRequest)
        return mCurrentRequest->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (mCurrentRequest)
        return mCurrentRequest->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
#endif
    if (mCurrentRequest)
        return mCurrentRequest->Resume();
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

nsresult
nsFileChannel::EnsureFile()
{
    if (mFile)
        return NS_OK;

    nsresult rv;
    // if we support the nsIURL interface then use it to get just
    // the file path with no other garbage!
    nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(mURI, &rv);
    NS_ENSURE_TRUE(fileURL, NS_ERROR_UNEXPECTED);

    rv = fileURL->GetFile(getter_AddRefs(mFile));
    if (NS_FAILED(rv)) return NS_ERROR_FILE_NOT_FOUND;

    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(mFile);
    if (localFile)
        localFile->SetFollowLinks(PR_TRUE);

    return NS_OK;
}
nsresult
nsFileChannel::GetFileTransport(nsITransport **trans)
{
    nsresult rv = EnsureFile();
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIFileTransportService> fts =
             do_GetService(kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mFile, mIOFlags, mPerm, PR_TRUE, trans);
    if (NS_FAILED(rv)) return rv;

    // XXX why should file:// URLs be loaded in the background?
    (*trans)->SetNotificationCallbacks(mCallbacks, (mLoadFlags & LOAD_BACKGROUND));
    return rv;
}

NS_IMETHODIMP
nsFileChannel::Open(nsIInputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS; // AsyncOpen in progress

    nsCOMPtr<nsITransport> fileTransport;
    rv = GetFileTransport(getter_AddRefs(fileTransport));
    if (NS_FAILED(rv)) return rv;

    if (mUploadStream) {
        // open output stream for "uploading"
        nsCOMPtr<nsIOutputStream> fileOut;
        rv = fileTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(fileOut));
        if (NS_FAILED(rv)) return rv;

        // write all of mUploadStream to fileOut before returning
        while (mUploadStreamLength) {
            PRUint32 bytesWritten = 0;

            rv = fileOut->WriteFrom(mUploadStream, mUploadStreamLength, &bytesWritten);
            if (NS_FAILED(rv)) return rv;

            if (bytesWritten == 0) {
                NS_WARNING("wrote zero bytes");
                return NS_ERROR_UNEXPECTED;
            }

            mUploadStreamLength -= bytesWritten;
        }
        if (NS_FAILED(rv)) return rv;

        // return empty stream to caller...
        nsCOMPtr<nsISupports> emptyIn;
        rv = NS_NewByteInputStream(getter_AddRefs(emptyIn), "", 0);
        if (NS_FAILED(rv)) return rv;

        rv = CallQueryInterface(emptyIn, result);
    }
    else
        rv = fileTransport->OpenInputStream(0, PRUint32(-1), 0, result);

    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv;
    nsCOMPtr<nsIRequest> request;

#ifdef DEBUG
    NS_ASSERTION(mInitiator == nsnull || mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
    mInitiator = PR_GetCurrentThread();
#endif

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS; // AsyncOpen in progress

    NS_ASSERTION(listener, "null listener");
    mRealListener = listener;

    if (mLoadGroup) {
        rv = mLoadGroup->AddRequest(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsITransport> fileTransport;
    rv = GetFileTransport(getter_AddRefs(fileTransport));

    if (NS_SUCCEEDED(rv)) {

        if (mUploadStream)
            rv = fileTransport->AsyncWrite(this, ctxt, 0, PRUint32(-1), 0,
                                           getter_AddRefs(request));
        else
            rv = fileTransport->AsyncRead(this, ctxt, 0, PRUint32(-1), 0,
                                          getter_AddRefs(request));

        // remember the transport and request; these will be released when
        // OnStopRequest is called.
        mFileTransport = fileTransport;
        mCurrentRequest = request;
    }

    if (NS_FAILED(rv)) {

        mStatus = rv;

        nsCOMPtr<nsIRequestObserver> asyncObserver;
        NS_NewRequestObserverProxy(getter_AddRefs(asyncObserver),
                                   NS_STATIC_CAST(nsIRequestObserver*, /* Ambiguous conversion */
                                   NS_STATIC_CAST(nsIStreamListener*, this)),
                                   NS_CURRENT_EVENTQ);
        if(asyncObserver) {
            (void) asyncObserver->OnStartRequest(this, ctxt);
            (void) asyncObserver->OnStopRequest(this, ctxt, rv);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentType(nsACString &aContentType)
{
    aContentType.Truncate();

    if (!mFile) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    if (mContentType.IsEmpty()) {
        PRBool directory;
        mFile->IsDirectory(&directory);
        if (directory) {
            if (mGenerateHTMLDirs)
                mContentType = NS_LITERAL_CSTRING(TEXT_HTML);
            else
                mContentType = NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT);
        }
        else {
            nsresult rv;
            nsCOMPtr<nsIMIMEService> MIMEService(do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
            if (NS_FAILED(rv)) return rv;

            nsXPIDLCString mimeType;
            rv = MIMEService->GetTypeFromFile(mFile, getter_Copies(mimeType));
            if (NS_SUCCEEDED(rv))
                mContentType = mimeType;
        }

        if (mContentType.IsEmpty())
            mContentType = NS_LITERAL_CSTRING(UNKNOWN_CONTENT_TYPE);
    }
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetContentType(const nsACString &aContentType)
{
    // only modifies mContentCharset if a charset is parsed
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
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetContentLength(PRInt32 *aContentLength)
{
    if (!mFile) {
        return NS_ERROR_NOT_AVAILABLE;
    }

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
// nsIRequestObserver methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnStartRequest(nsIRequest* request, nsISupports* context)
{
    // unconditionally inherit the status of the file transport
    request->GetStatus(&mStatus);
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
#endif
    NS_ASSERTION(mRealListener, "No listener...");
    nsresult rv = NS_OK;
    if (mRealListener) {
        if (mGenerateHTMLDirs) {
            // GetFileTransport ensures that mFile is valid before calling
            // AsyncRead or AsyncWrite on the underlying transport.  Since
            // these transports use |this| as the nsIRequestObserver, there
            // should be no way that mFile is null here.
            NS_ENSURE_TRUE(mFile, NS_ERROR_UNEXPECTED);
            PRBool directory;
            mFile->IsDirectory(&directory);  // this stat should be cached and will not hit disk.
            if (directory) {
                rv = SetStreamConverter();
                if (NS_FAILED(rv))
                    return rv;
            }
        }

        rv = mRealListener->OnStartRequest(this, context);
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OnStopRequest(nsIRequest* request, nsISupports* context,
                             nsresult aStatus)
{
    // if we were canceled, we should not overwrite the cancelation status
    // since that could break security assumptions made in upper layers.
    if (NS_SUCCEEDED(mStatus))
        mStatus = aStatus;

#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
#endif

    NS_ENSURE_TRUE(mRealListener, NS_ERROR_UNEXPECTED);

    // no point to capturing the return value of OnStopRequest
    mRealListener->OnStopRequest(this, context, mStatus);

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, context, mStatus);

    // Release the reference to the consumer stream listener...
    mRealListener = 0;
    mFileTransport = 0;
    mCurrentRequest = 0;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnDataAvailable(nsIRequest* request, nsISupports* context,
                               nsIInputStream *aIStream, PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_GetCurrentThread(),
                 "wrong thread calling this routine");
#endif

    NS_ENSURE_TRUE(mRealListener, NS_ERROR_UNEXPECTED);

    return mRealListener->OnDataAvailable(this, context, aIStream,
                                          aSourceOffset, aLength);
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamProvider methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnDataWritable(nsIRequest *aRequest, nsISupports *aContext,
                              nsIOutputStream *aOStream, PRUint32 aOffset,
                              PRUint32 aLength)
{
#ifdef DEBUG
    NS_ASSERTION(mInitiator == PR_CurrentThread(),
                 "wrong thread calling this routine");
#endif

    NS_ENSURE_TRUE(mUploadStream, NS_ERROR_UNEXPECTED);

    if (mUploadStreamLength == 0)
        return NS_BASE_STREAM_CLOSED; // done writing

    PRUint32 bytesToWrite = PR_MIN(mUploadStreamLength, aLength);
    PRUint32 bytesWritten = 0;

    nsresult rv = aOStream->WriteFrom(mUploadStream, bytesToWrite, &bytesWritten);
    if (NS_FAILED(rv)) return rv;

    if (bytesWritten == 0) {
        NS_WARNING("wrote zero bytes");
        return NS_BASE_STREAM_CLOSED;
    }

    mUploadStreamLength -= bytesWritten;
    return NS_OK;
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
nsFileChannel::OnStatus(nsIRequest *request, nsISupports* ctxt,
                        nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv = NS_OK;
    if (mProgress) {
        rv = mProgress->OnStatus(this, ctxt, aStatus, aStatusArg);
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OnProgress(nsIRequest *request,
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
    nsCOMPtr<nsIFileURL> url = new nsStandardURL(PR_TRUE);
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;

    // XXX shouldn't we set nsIURI::originCharset ??

    rv = url->SetFile(file);
    if (NS_FAILED(rv)) return rv;

    return Init(ioFlags, perm, url);
}

NS_IMETHODIMP
nsFileChannel::GetFile(nsIFile* *result)
{
    nsresult rv = EnsureFile();
    if (NS_FAILED(rv))
        return rv;

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
// From nsIUploadChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::SetUploadStream(nsIInputStream *aStream,
                               const nsACString &aContentType,
                               PRInt32 aContentLength)
{
    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS; // channel is pending, so we can't add
                                     // or remove an upload stream.

    // ignore aContentType argument; query the stream for its length if not
    // specified (allow upload of a partial stream).

    if (aContentLength < 0) {
        nsresult rv = aStream->Available(&mUploadStreamLength);
        if (NS_FAILED(rv)) return rv;
    }
    else
        mUploadStreamLength = aContentLength;
    mUploadStream = aStream;

    if (mUploadStream) {
        // configure mIOFlags for writing
        mIOFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
        if (mPerm == -1)
            mPerm = PR_IRUSR | PR_IWUSR; // pick something reasonable
    }
    else {
        // configure mIOFlags for reading
        mIOFlags = PR_RDONLY;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetUploadStream(nsIInputStream **stream)
{
    NS_ENSURE_ARG_POINTER(stream);
    *stream = mUploadStream;
    NS_IF_ADDREF(*stream);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
