/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

// data implementation

#include "nsDataChannel.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsNetUtil.h"
#include "nsILoadGroup.h"
#include "plbase64.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


// nsDataChannel methods
nsDataChannel::nsDataChannel() {
    mStatus = NS_OK;
    mContentLength = -1;
    mLoadFlags = nsIRequest::LOAD_NORMAL;
}

nsDataChannel::~nsDataChannel() {
}

NS_IMPL_ISUPPORTS5(nsDataChannel,
                   nsIDataChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIRequestObserver,
                   nsIStreamListener)

nsresult
nsDataChannel::Init(nsIURI* uri)
{
    nsresult rv;

    // Data urls contain all the data within the url string itself.
    mUrl = uri;

    rv = ParseData();

    return rv;
}

typedef struct _writeData {
    PRUint32 dataLen;
    char *data;
} writeData;

static NS_METHOD
nsReadData(nsIOutputStream* out,
           void* closure, // the data from
           char* toRawSegment, // where to put the data
           PRUint32 offset, // where to start
           PRUint32 count, // how much data is there
           PRUint32 *readCount) { // how much data was read
  nsresult rv = NS_OK;
  writeData *dataToWrite = (writeData*)closure;
  PRUint32 write = PR_MIN(count, dataToWrite->dataLen - offset);

  *readCount = 0;

  if (offset == dataToWrite->dataLen)
      return NS_OK;     // *readCount == 0 is EOF

  memcpy(toRawSegment, dataToWrite->data + offset, write);

  *readCount = write;

  return rv;
}

nsresult
nsDataChannel::ParseData() {
    nsresult rv;
    PRBool lBase64 = PR_FALSE;
    NS_ASSERTION(mUrl, "no url in the data channel");
    if (!mUrl) return NS_ERROR_NULL_POINTER;

    nsCAutoString spec;
    rv = mUrl->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    // move past "data:"
    const char *buffer = strstr(spec.get(), "data:");
    if (!buffer) {
        // malfored url
        return NS_ERROR_MALFORMED_URI;
    }
    buffer += 5;

    // First, find the start of the data
    char *comma = PL_strchr(buffer, ',');
    if (!comma) return NS_ERROR_FAILURE;

    *comma = '\0';

    // determine if the data is base64 encoded.
    char *base64 = PL_strstr(buffer, ";base64");
    if (base64) {
        lBase64 = PR_TRUE;
        *base64 = '\0';
    }

    if (comma == buffer) {
        // nothing but data
        mContentType = NS_LITERAL_CSTRING("text/plain");
        mContentCharset = NS_LITERAL_CSTRING("US-ASCII");
    } else {
        // everything else is content type
        char *semiColon = PL_strchr(buffer, ';');
        if (semiColon)
            *semiColon = '\0';
        
        if (semiColon == buffer || base64 == buffer) {
          // there is no content type, but there are other parameters
          mContentType = NS_LITERAL_CSTRING("text/plain");
        } else {
          mContentType = buffer;
          ToLowerCase(mContentType);
        }

        if (semiColon) {
            char *charset = PL_strcasestr(semiColon + 1, "charset=");
            if (charset)
                mContentCharset = charset + sizeof("charset=") - 1;

            *semiColon = ';';
        }
    }
    mContentType.StripWhitespace();
    mContentCharset.StripWhitespace();

    char *dataBuffer = nsnull;
    PRBool cleanup = PR_FALSE;
    if (!lBase64 && ((strncmp(mContentType.get(),"text/",5) == 0) || mContentType.Find("xml") != kNotFound)) {
        // it's text, don't compress spaces
        dataBuffer = comma+1;
    } else {
        // it's ascii encoded binary, don't let any spaces in
        nsCAutoString dataBuf(comma+1);
        dataBuf.StripWhitespace();
        dataBuffer = ToNewCString(dataBuf);
        if (!dataBuffer)
            return NS_ERROR_OUT_OF_MEMORY;
        cleanup = PR_TRUE;
    }
    
    nsCOMPtr<nsIInputStream> bufInStream;
    nsCOMPtr<nsIOutputStream> bufOutStream;
    writeData* dataToWrite = nsnull;
    PRUint32 dataLen = PL_strlen(dataBuffer);
    
    rv = NS_NewPipe(getter_AddRefs(bufInStream), getter_AddRefs(bufOutStream));
    if (NS_FAILED(rv))
        goto cleanup;

    PRUint32 wrote;
    dataToWrite = (writeData*)nsMemory::Alloc(sizeof(writeData));
    if (!dataToWrite) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    if (lBase64) {
        *base64 = ';';
        PRInt32 resultLen = 0;
        if (dataBuffer[dataLen-1] == '=') {
            if (dataBuffer[dataLen-2] == '=')
                resultLen = dataLen-2;
            else
                resultLen = dataLen-1;
        } else {
            resultLen = dataLen;
        }
        resultLen = ((resultLen * 3) / 4);

        // XXX PL_Base64Decode will return a null pointer for decoding
        // errors.  Since those are more likely than out-of-memory,
        // should we return NS_ERROR_MALFORMED_URI instead?
        char * decodedData = PL_Base64Decode(dataBuffer, dataLen, nsnull);
        if (!decodedData) {
            rv = NS_ERROR_OUT_OF_MEMORY;
            goto cleanup;
        }

        dataToWrite->dataLen = resultLen;
        dataToWrite->data = decodedData;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataToWrite->dataLen, &wrote);

        nsMemory::Free(decodedData);
    } else {
        dataToWrite->dataLen = nsUnescapeCount(dataBuffer);
        dataToWrite->data = dataBuffer;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataLen, &wrote);
    }
    if (NS_FAILED(rv))
        goto cleanup;

    // Initialize the content length of the data...
    mContentLength = dataToWrite->dataLen;

    rv = bufInStream->QueryInterface(NS_GET_IID(nsIInputStream), getter_AddRefs(mDataStream));
    if (NS_FAILED(rv))
        goto cleanup;

    *comma = ',';

    rv = NS_OK;

 cleanup:
    if (dataToWrite)
        nsMemory::Free(dataToWrite);
    if (cleanup)
        nsMemory::Free(dataBuffer);
    return rv;
}

NS_METHOD
nsDataChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsDataChannel* dc = new nsDataChannel();
    if (dc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dc);
    nsresult rv = dc->QueryInterface(aIID, aResult);
    NS_RELEASE(dc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsDataChannel::GetName(nsACString &result)
{
    if (mUrl)
        return mUrl->GetSpec(result);
    result.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::IsPending(PRBool *result)
{
    // This request is pending if there is an active stream listener...

    *result = (mListener) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");

    mStatus = status;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::Suspend(void)
{
    // This channel is not suspendable...
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDataChannel::Resume(void)
{
    // This channel is not resumable...
    return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsDataChannel::GetOriginalURI(nsIURI* *aURI)
{
    *aURI = mOriginalURI ? mOriginalURI : mUrl;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetOriginalURI(nsIURI* aURI)
{
    mOriginalURI = aURI;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetURI(nsIURI* *aURI)
{
    *aURI = mUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

// This class 

NS_IMETHODIMP
nsDataChannel::Open(nsIInputStream **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    NS_ADDREF(*_retval = mDataStream);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
    nsresult rv;
    nsCOMPtr<nsIEventQueue> eventQ;
    nsCOMPtr<nsIStreamListener> listener;

    nsCOMPtr<nsIEventQueueService> eventQService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;

    // we'll just fire everything off at once because we've already got all
    // the data.
    rv = NS_NewAsyncStreamListener(getter_AddRefs(listener), this, eventQ);
    if (NS_FAILED(rv)) return rv;

    // Hold onto the real consumer...
    mListener = aListener;

    // Add the request to the loadgroup (if available)
    if (mLoadGroup) {
        mLoadGroup->AddRequest(this, nsnull);
    }

    // Next, queue up asynchronous stream notifications for OnStartRequest,
    // a single OnDataAvailable (containing all of the data) and an
    // OnStopRequest...
    //
    // These notifications will be processed when control returns to the
    // message pump and the PLEvents are processed...
    //
    mStatus = listener->OnStartRequest(this, ctxt);

    // Only fire OnDataAvailable(...) if OnStartRequest(...) succeeded!
    if (NS_SUCCEEDED(mStatus)) {
        mStatus = listener->OnDataAvailable(this, ctxt, mDataStream,
                                            0, mContentLength);
    }
    // Always fire OnStopRequest(...) even if an error occurred!
    (void) listener->OnStopRequest(this, ctxt, mStatus);

    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetContentType(nsACString &aContentType)
{
    NS_ASSERTION(mContentType.Length() > 0, "data protocol should have content type by now");
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetContentType(const nsACString &aContentType)
{
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetContentCharset(const nsACString &aContentCharset)
{
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetContentLength(PRInt32 aContentLength)
{
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    NS_ENSURE_ARG_POINTER(aNotificationCallbacks);
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetSecurityInfo(nsISupports **sec)
{
    NS_ENSURE_ARG_POINTER(sec);
    *sec = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver methods:

NS_IMETHODIMP
nsDataChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    if (NS_SUCCEEDED(mStatus)) {
        mStatus = mListener->OnStartRequest(request, ctxt);
    }

    return mStatus;
}

NS_IMETHODIMP
nsDataChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                             nsresult status)
{
    // If the request has already failed for some reason, then pass out that
    // failure code...
    //
    if (NS_SUCCEEDED(mStatus)) {
        mStatus = status;
    }

    if (mListener) {
        (void) mListener->OnStopRequest(request, ctxt, mStatus);
    }

    // Drop the reference to the stream listener -- it is no longer needed.
    mListener = nsnull;

    // Remove this request from the load group -- it is complete.
    if (mLoadGroup) {
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
    }

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsDataChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *input,
                               PRUint32 offset, PRUint32 count)
{
    if (NS_SUCCEEDED(mStatus)) {
        mStatus = mListener->OnDataAvailable(request, ctxt, input,
                                             offset, count);
    }

    return mStatus;
}

