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
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
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
nsDataChannel::Init(nsIURI* uri)
{
    // we don't care about event sinks in data
    nsresult rv;

    // Data urls contain all the data within the url string itself.
    mUrl = uri;

    rv = ParseData();
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
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
        mContentType = cType.get();

        if (semiColon)
            *semiColon = ';';
    }

    char *dataBuffer = nsnull;
    PRBool cleanup = PR_FALSE;
    mContentType.StripWhitespace();
    if (!mContentType.Find("text") && !lBase64) {
        // it's text, don't compress spaces
        dataBuffer = comma+1;
    } else {
        // it's ascii encoded binary, don't let any spaces in
        nsCAutoString dataBuf(comma+1);
        dataBuf.StripWhitespace();
        dataBuffer = dataBuf.ToNewCString();
        cleanup = PR_TRUE;
    }
    
    nsCOMPtr<nsIInputStream> bufInStream;
    nsCOMPtr<nsIOutputStream> bufOutStream;

    rv = NS_NewPipe(getter_AddRefs(bufInStream), getter_AddRefs(bufOutStream));
    if (NS_FAILED(rv)) return rv;

    PRUint32 dataLen = PL_strlen(dataBuffer);
    PRUint32 wrote;
    writeData *dataToWrite = (writeData*)nsMemory::Alloc(sizeof(writeData));
    if (!dataToWrite) return NS_ERROR_OUT_OF_MEMORY;

    if (lBase64) {
        *base64 = 'b';
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

        char * decodedData = PL_Base64Decode(dataBuffer, dataLen, nsnull);
        if (!decodedData) return NS_ERROR_OUT_OF_MEMORY;

        dataToWrite->dataLen = resultLen;
        dataToWrite->data = decodedData;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataToWrite->dataLen, &wrote);

        nsMemory::Free(decodedData);
    } else {
        dataToWrite->dataLen = dataLen;
        dataToWrite->data = dataBuffer;

        rv = bufOutStream->WriteSegments(nsReadData, dataToWrite, dataLen, &wrote);
    }
    if (NS_FAILED(rv)) return rv;

    // Initialize the content length of the data...
    mContentLength = dataToWrite->dataLen;

    rv = bufInStream->QueryInterface(NS_GET_IID(nsIInputStream), getter_AddRefs(mDataStream));
    if (NS_FAILED(rv)) return rv;

    *comma = ',';

    nsMemory::Free(dataToWrite);
    if (cleanup) nsMemory::Free(dataBuffer);
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
nsDataChannel::GetName(PRUnichar* *result)
{
    NS_NOTREACHED("nsDataChannel::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::IsPending(PRBool *result)
{
    NS_NOTREACHED("nsDataChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::GetStatus(nsresult *status)
{
    *status = NS_OK;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    NS_NOTREACHED("nsDataChannel::Cancel");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::Suspend(void)
{
    NS_NOTREACHED("nsDataChannel::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDataChannel::Resume(void)
{
    NS_NOTREACHED("nsDataChannel::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    rv = NS_NewAsyncStreamListener(getter_AddRefs(listener), aListener, eventQ);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnStartRequest(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    PRUint32 streamLen;
    rv = mDataStream->Available(&streamLen);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnDataAvailable(this, ctxt, mDataStream, 0, streamLen);
    if (NS_FAILED(rv)) return rv;

    rv = listener->OnStopRequest(this, ctxt, NS_OK);
    return rv;
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
