/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

// data implementation

#include "nsDataChannel.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsNetUtil.h"
#include "nsILoadGroup.h"
#include "plbase64.h"
#include "prmem.h"
#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNetSegmentUtils.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsNetCID.h"

// nsDataChannel methods
nsDataChannel::nsDataChannel() :
    mContentLength(-1),
    mOpened(PR_FALSE)
{
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
    // Data urls contain all the data within the url string itself.
    mUrl = uri;

    nsresult rv;
    mPump = do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
    if (NS_FAILED(rv))
      return rv;

    rv = ParseData();

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
    char *buffer = (char *) strstr(spec.BeginWriting(), "data:");
    if (!buffer) {
        // malfored url
        return NS_ERROR_MALFORMED_URI;
    }
    buffer += 5;

    // First, find the start of the data
    char *comma = strchr(buffer, ',');
    if (!comma) return NS_ERROR_FAILURE;

    *comma = '\0';

    // determine if the data is base64 encoded.
    char *base64 = strstr(buffer, ";base64");
    if (base64) {
        lBase64 = PR_TRUE;
        *base64 = '\0';
    }

    if (comma == buffer) {
        // nothing but data
        mContentType.AssignLiteral("text/plain");
        mContentCharset.AssignLiteral("US-ASCII");
    } else {
        // everything else is content type
        char *semiColon = (char *) strchr(buffer, ';');
        if (semiColon)
            *semiColon = '\0';
        
        if (semiColon == buffer || base64 == buffer) {
          // there is no content type, but there are other parameters
          mContentType.AssignLiteral("text/plain");
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
    
    // create an unbounded pipe.
    rv = NS_NewPipe(getter_AddRefs(bufInStream),
                    getter_AddRefs(bufOutStream),
                    NET_DEFAULT_SEGMENT_SIZE, PR_UINT32_MAX,
                    PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv))
        goto cleanup;

    PRUint32 dataLen;
    dataLen = nsUnescapeCount(dataBuffer);
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

        rv = bufOutStream->Write(decodedData, resultLen, (PRUint32*)&mContentLength);

        PR_Free(decodedData);
    } else {
        rv = bufOutStream->Write(dataBuffer, dataLen, (PRUint32*)&mContentLength);
    }
    if (NS_FAILED(rv))
        goto cleanup;

    rv = bufInStream->QueryInterface(NS_GET_IID(nsIInputStream), getter_AddRefs(mDataStream));
    if (NS_FAILED(rv))
        goto cleanup;

    *comma = ',';

    rv = NS_OK;

 cleanup:
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
    return mPump->IsPending(result);
}

NS_IMETHODIMP
nsDataChannel::GetStatus(nsresult *status)
{
    return mPump->GetStatus(status);
}

NS_IMETHODIMP
nsDataChannel::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");

    return mPump->Cancel(status);
}

NS_IMETHODIMP
nsDataChannel::Suspend(void)
{
    return mPump->Suspend();
}

NS_IMETHODIMP
nsDataChannel::Resume(void)
{
    return mPump->Resume();
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
nsDataChannel::GetLoadFlags(PRUint32 *aLoadFlags)
{
    return mPump->GetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsDataChannel::SetLoadFlags(PRUint32 aLoadFlags)
{
    return mPump->SetLoadFlags(aLoadFlags);
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
    mOpened = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
    // Hold onto the real consumer...
    mListener = aListener;
    mOpened = PR_TRUE;

    nsresult rv = mPump->Init(mDataStream, -1, -1, 0, 0, PR_FALSE);
    if (NS_FAILED(rv))
        return rv;

    // Add to load group
    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    // Start the read
    return mPump->AsyncRead(this, ctxt);

    // These notifications will be processed when control returns to the
    // message pump and the PLEvents are processed...
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
    // Ignore content-type hints
    if (mOpened) {
        mContentType = aContentType;
    }
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
    // Ignore content-charset hints
    if (mOpened) {
        mContentCharset = aContentCharset;
    }
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
    if (mListener) {
        return mListener->OnStartRequest(this, ctxt);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsDataChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                             nsresult status)
{
    if (mListener) {
        mListener->OnStopRequest(this, ctxt, status);

        // Drop the reference to the stream listener -- it is no longer needed.
        mListener = nsnull;
    }

    // Remove from Loadgroup
    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsDataChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *input,
                               PRUint32 offset, PRUint32 count)
{
    if (mListener) {
        return mListener->OnDataAvailable(this, ctxt, input,
                                          offset, count);
    }
    return NS_OK;
}

