/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// data implementation

#include "nsDataChannel.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsNetUtil.h"
#include "nsILoadGroup.h"
#include "plbase64.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPipe.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);


// nsDataChannel methods
nsDataChannel::nsDataChannel() {
    NS_INIT_REFCNT();

    mContentLength = -1;
}

nsDataChannel::~nsDataChannel() {
}

NS_IMPL_ISUPPORTS3(nsDataChannel,
                   nsIDataChannel,
                   nsIChannel,
                   nsIRequest)

nsresult
nsDataChannel::Init(const char* verb, 
                    nsIURI* uri, 
                    nsILoadGroup* aLoadGroup,
                    nsIInterfaceRequestor* notificationCallbacks, 
                    nsLoadFlags loadAttributes,
                    nsIURI* originalURI,
                    PRUint32 bufferSegmentSize,
                    PRUint32 bufferMaxSize)
{
    // we don't care about event sinks in data
    nsresult rv;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    // Data urls contain all the data within the url string itself.
    mOriginalURI = originalURI ? originalURI : uri;
    mUrl = uri;
    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;

    rv = ParseData();
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

typedef struct _writeData {
    PRUint32 dataLen;
    char *data;
} writeData;

NS_METHOD
nsReadData(void* closure, // the data from
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

  nsCRT::memcpy(toRawSegment, dataToWrite->data + offset, write);

  *readCount = write;

  return rv;
}

nsresult
nsDataChannel::ParseData() {
    nsresult rv;
    PRBool lBase64 = PR_FALSE;
    NS_ASSERTION(mUrl, "no url in the data channel");
    if (!mUrl) return NS_ERROR_NULL_POINTER;

    nsXPIDLCString spec;
    rv = mUrl->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    // move past "data:"
    char *buffer = PL_strstr((const char*)spec, "data:");
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
    char *base64 = PL_strstr(buffer, "base64");
    if (base64) {
        lBase64 = PR_TRUE;
        *base64 = '\0';
    }

    if (comma == buffer) {
        // nothing but data
        mContentType = "text/plain;charset=US-ASCII";
    } else {
        // everything else is content type
        char *semiColon = PL_strchr(buffer, ';');
        if (semiColon)
            *semiColon = '\0';

        nsCAutoString cType(buffer);
        cType.ToLowerCase();
        mContentType = cType.GetBuffer();

        if (semiColon)
            *semiColon = ';';
    }
    
    nsCOMPtr<nsIBufferInputStream> bufInStream;
    nsCOMPtr<nsIBufferOutputStream> bufOutStream;

    rv = NS_NewPipe(getter_AddRefs(bufInStream), getter_AddRefs(bufOutStream));
    if (NS_FAILED(rv)) return rv;

    PRUint32 dataLen = PL_strlen(comma+1);
    PRUint32 wrote;
    writeData *dataToWrite = (writeData*)nsAllocator::Alloc(sizeof(writeData));
    if (!dataToWrite) return NS_ERROR_OUT_OF_MEMORY;

    if (lBase64) {
        *base64 = 'b';
        PRInt32 resultLen = 0;
        if (comma[dataLen-1] == '=')
            if (comma[dataLen-2] == '=')
                resultLen = dataLen-2;
            else
                resultLen = dataLen-1;

        char * decodedData = PL_Base64Decode(comma+1, dataLen, nsnull);
        if (!decodedData) return NS_ERROR_OUT_OF_MEMORY;

        dataToWrite->dataLen = resultLen;
        dataToWrite->data = decodedData;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataToWrite->dataLen, &wrote);

        nsAllocator::Free(decodedData);
    } else {
        dataToWrite->dataLen = dataLen;
        dataToWrite->data = comma + 1;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataLen, &wrote);
    }
    if (NS_FAILED(rv)) return rv;

    // Initialize the content length of the data...
    mContentLength = dataToWrite->dataLen;

    rv = bufInStream->QueryInterface(NS_GET_IID(nsIInputStream), getter_AddRefs(mDataStream));
    if (NS_FAILED(rv)) return rv;

    *comma = ',';

    nsAllocator::Free(dataToWrite);
    return NS_OK;
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
nsDataChannel::IsPending(PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::Cancel(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::Suspend(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::Resume(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsDataChannel::GetOriginalURI(nsIURI * *aURI)
{
    *aURI = mOriginalURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetURI(nsIURI * *aURI)
{
    *aURI = mUrl;
    NS_IF_ADDREF(*aURI);
    return NS_OK;
}

// This class 

NS_IMETHODIMP
nsDataChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                              nsIInputStream **_retval)
{
    // XXX we should honor the startPosition and count passed in.

    *_retval = mDataStream;
    NS_ADDREF(*_retval);
    
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    // you can't write to a data url
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                        nsISupports *ctxt,
                        nsIStreamListener *aListener)
{
    nsresult rv;
    nsCOMPtr<nsIEventQueue> eventQ;
    nsCOMPtr<nsIStreamListener> listener;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;

    // we'll just fire everything off at once because we've already got all
    // the data.
    rv = NS_NewAsyncStreamListener(aListener, eventQ, getter_AddRefs(listener));
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnStartRequest(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    PRUint32 streamLen;
    rv = mDataStream->Available(&streamLen);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnDataAvailable(this, ctxt, mDataStream, 0, streamLen);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnStopRequest(this, ctxt, NS_OK, nsnull);

    return rv;
}

NS_IMETHODIMP
nsDataChannel::AsyncWrite(nsIInputStream *fromStream,
                         PRUint32 startPosition,
                         PRInt32 writeCount,
                         nsISupports *ctxt,
                         nsIStreamObserver *observer)
{
    // you can't write to a data url
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetContentType(char* *aContentType) {
    // Parameter validation...
    if (!aContentType) return NS_ERROR_NULL_POINTER;

    if (mContentType.Length()) {
        *aContentType = mContentType.ToNewCString();
        if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
    } else {
        NS_ASSERTION(0, "data protocol should have content type by now");
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetContentType(const char *aContentType)
{
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
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
nsDataChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}
