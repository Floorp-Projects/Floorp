/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// data implementation

#include "nsDataChannel.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIPrincipal.h"
#include "plbase64.h"
#include "nsIEventSinkGetter.h"
#include "nsIPipe.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);


// nsDataChannel methods
nsDataChannel::nsDataChannel() {
    NS_INIT_REFCNT();
}

nsDataChannel::~nsDataChannel() {
    NS_IF_RELEASE(mDataStream);
    NS_RELEASE(mUrl);
    NS_IF_RELEASE(mLoadGroup);
}

NS_IMPL_ADDREF(nsDataChannel);
NS_IMPL_RELEASE(nsDataChannel);

NS_IMETHODIMP
nsDataChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(NS_GET_IID(nsIDataChannel)) ||
        aIID.Equals(NS_GET_IID(nsIChannel)) ||
        aIID.Equals(NS_GET_IID(nsISupports)) ) {
        *aInstancePtr = NS_STATIC_CAST(nsIDataChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

nsresult
nsDataChannel::Init(const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                   nsIEventSinkGetter* getter) {
    // we don't care about event sinks in data
    nsresult rv;

    mDataStream = nsnull;

    // Data urls contain all the data within the url string itself.
    mUrl = uri;
    NS_ADDREF(mUrl);

    rv = ParseData();
    if (NS_FAILED(rv)) return rv;

    mLoadGroup = aGroup;
    NS_IF_ADDREF(mLoadGroup);

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
      return NS_BASE_STREAM_EOF;

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

    char *spec;
    rv = mUrl->GetSpec(&spec);
    if (NS_FAILED(rv)) return rv;

    // move past "data:"
    char *buffer = PL_strstr(spec, "data:");
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

        mContentType = buffer;

        if (semiColon)
            *semiColon = ';';
    }
    
    nsIBufferInputStream *bufInStream;
    nsIBufferOutputStream *bufOutStream;

    rv = NS_NewPipe(&bufInStream, &bufOutStream);
    if (NS_FAILED(rv)) return rv;

    PRUint32 dataLen = PL_strlen(comma+1);
    PRUint32 wrote;
    writeData *dataToWrite = (writeData*)nsAllocator::Alloc(sizeof(dataToWrite));
    if (!dataToWrite) return NS_ERROR_OUT_OF_MEMORY;

    if (lBase64) {
        *base64 = 'b';
        char *decodedData = (char*)nsAllocator::Alloc((dataLen * 3)/4);
        if (!decodedData) return NS_ERROR_OUT_OF_MEMORY;

        decodedData = PL_Base64Decode(comma+1, dataLen, decodedData);

        dataToWrite->dataLen = (dataLen * 3)/4;
        dataToWrite->data = decodedData;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, (dataLen * 3)/4, &wrote);

        nsAllocator::Free(decodedData);
    } else {
        dataToWrite->dataLen = dataLen;
        dataToWrite->data = comma + 1;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataLen, &wrote);
    }
    if (NS_FAILED(rv)) return rv;


    rv = bufInStream->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&mDataStream);
    NS_RELEASE(bufInStream);
    NS_RELEASE(bufOutStream);
    if (NS_FAILED(rv)) return rv;

    *comma = ',';

    nsAllocator::Free(spec);

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
nsDataChannel::GetURI(nsIURI * *aURI)
{
    NS_ADDREF(mUrl);
    *aURI = mUrl;
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
nsDataChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                        nsISupports *ctxt,
                        nsIStreamListener *aListener)
{
    nsresult rv;
    nsIEventQueue *eventQ;
    nsIStreamListener *listener;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // we'll just fire everything off at once because we've already got all
    // the data.
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &eventQ);
    if (NS_FAILED(rv)) return rv;

    rv = serv->NewAsyncStreamListener(aListener, eventQ, &listener);
    NS_RELEASE(eventQ);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnStartRequest(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    PRUint32 streamLen;
    rv = mDataStream->GetLength(&streamLen);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnDataAvailable(this, ctxt, mDataStream, 0, streamLen);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnStopRequest(this, ctxt, NS_OK, nsnull);
    NS_RELEASE(listener);

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
nsDataChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::GetPrincipal(nsIPrincipal * *aPrincipal)
{
    *aPrincipal = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::SetPrincipal(nsIPrincipal * aPrincipal)
{
    return NS_OK;
}
