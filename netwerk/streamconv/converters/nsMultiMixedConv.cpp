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

#include "nsMultiMixedConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "nsIHttpChannel.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIByteArrayInputStream.h"
#include "nsReadableUtils.h"


//
// nsPartChannel is a "dummy" channel which represents an individual part of
// a multipart/mixed stream...
//
// Instances on this channel are passed out to the consumer through the
// nsIStreamListener interface.
//
class nsPartChannel : public nsIChannel,
                      public nsIByteRangeRequest
{
public:
  nsPartChannel(nsIChannel *aMultipartChannel);

  void InitializeByteRange(PRInt32 aStart, PRInt32 aEnd);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL
  NS_DECL_NSIBYTERANGEREQUEST

protected:
  virtual ~nsPartChannel();

protected:
  nsCOMPtr<nsIChannel>    mMultipartChannel;
  
  nsresult                mStatus;
  nsLoadFlags             mLoadFlags;

  nsCOMPtr<nsILoadGroup>  mLoadGroup;

  nsCString               mContentType;
  PRInt32                 mContentLength;

  PRBool                  mIsByteRangeRequest;
  PRInt32                 mByteRangeStart;
  PRInt32                 mByteRangeEnd;
};

nsPartChannel::nsPartChannel(nsIChannel *aMultipartChannel) :
  mStatus(NS_OK),
  mContentLength(-1),
  mIsByteRangeRequest(PR_FALSE),
  mByteRangeStart(0),
  mByteRangeEnd(0)
{
    NS_INIT_ISUPPORTS();

    mMultipartChannel = aMultipartChannel;

    // Inherit the load flags from the original channel...
    mMultipartChannel->GetLoadFlags(&mLoadFlags);

    mMultipartChannel->GetLoadGroup(getter_AddRefs(mLoadGroup));
}

nsPartChannel::~nsPartChannel()
{
}

void nsPartChannel::InitializeByteRange(PRInt32 aStart, PRInt32 aEnd)
{
    mIsByteRangeRequest = PR_TRUE;
    
    mByteRangeStart = aStart;
    mByteRangeEnd   = aEnd;
}


//
// nsISupports implementation...
//

NS_IMPL_ADDREF(nsPartChannel)
NS_IMPL_RELEASE(nsPartChannel)

NS_INTERFACE_MAP_BEGIN(nsPartChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIByteRangeRequest)
NS_INTERFACE_MAP_END

//
// nsIRequest implementation...
//

NS_IMETHODIMP
nsPartChannel::GetName(PRUnichar * *aResult)
{
    return mMultipartChannel->GetName(aResult);
}

NS_IMETHODIMP
nsPartChannel::IsPending(PRBool *aResult)
{
    // For now, consider the active lifetime of each part the same as
    // the underlying multipart channel...  This is not exactly right,
    // but it is good enough :-)
    return mMultipartChannel->IsPending(aResult);
}

NS_IMETHODIMP
nsPartChannel::GetStatus(nsresult *aResult)
{
    nsresult rv = NS_OK;

    if (NS_FAILED(mStatus)) {
        *aResult = mStatus;
    } else {
        rv = mMultipartChannel->GetStatus(aResult);
    }

    return rv;
}

NS_IMETHODIMP
nsPartChannel::Cancel(nsresult aStatus)
{
    // Cancelling an individual part must not cancel the underlying
    // multipart channel...
    mStatus = aStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::Suspend(void)
{
    // Suspending an individual part must not suspend the underlying
    // multipart channel...
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::Resume(void)
{
    // Resuming an individual part must not resume the underlying
    // multipart channel...
    return NS_OK;
}

//
// nsIChannel implementation
//

NS_IMETHODIMP
nsPartChannel::GetOriginalURI(nsIURI * *aURI)
{
    return mMultipartChannel->GetOriginalURI(aURI);
}

NS_IMETHODIMP
nsPartChannel::SetOriginalURI(nsIURI *aURI)
{
    return mMultipartChannel->SetOriginalURI(aURI);
}

NS_IMETHODIMP
nsPartChannel::GetURI(nsIURI * *aURI)
{
    return mMultipartChannel->GetURI(aURI);
}

NS_IMETHODIMP
nsPartChannel::Open(nsIInputStream **result)
{
    // This channel cannot be opened!
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPartChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
    // This channel cannot be opened!
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPartChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);

    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;

    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetOwner(nsISupports* *aOwner)
{
    return mMultipartChannel->GetOwner(aOwner);
}

NS_IMETHODIMP
nsPartChannel::SetOwner(nsISupports* aOwner)
{
    return mMultipartChannel->SetOwner(aOwner);
}

NS_IMETHODIMP
nsPartChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
    return mMultipartChannel->GetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsPartChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    return mMultipartChannel->SetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP 
nsPartChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return mMultipartChannel->GetSecurityInfo(aSecurityInfo);
}

NS_IMETHODIMP
nsPartChannel::GetContentType(char * *aContentType)
{
    *aContentType = ToNewCString(mContentType);
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentType(const char *aContentType)
{
    mContentType.Assign(aContentType);
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentLength(PRInt32 aContentLength)
{
    mContentLength = aContentLength;
    return NS_OK;
}


//
// nsIByteRangeRequest implementation...
//

NS_IMETHODIMP 
nsPartChannel::GetIsByteRangeRequest(PRBool *aIsByteRangeRequest)
{
    *aIsByteRangeRequest = mIsByteRangeRequest;

    return NS_OK;
}


NS_IMETHODIMP 
nsPartChannel::GetStartRange(PRInt32 *aStartRange)
{
    *aStartRange = mByteRangeStart;

    return NS_OK;
}

NS_IMETHODIMP 
nsPartChannel::GetEndRange(PRInt32 *aEndRange)
{
    *aEndRange = mByteRangeEnd;
    return NS_OK;
}





// nsISupports implementation
NS_IMPL_THREADSAFE_ISUPPORTS3(nsMultiMixedConv,
                              nsIStreamConverter, 
                              nsIStreamListener,
                              nsIRequestObserver);


// nsIStreamConverter implementation

// No syncronous conversion at this time.
NS_IMETHODIMP
nsMultiMixedConv::Convert(nsIInputStream *aFromStream,
                          const PRUnichar *aFromType,
                          const PRUnichar *aToType,
                          nsISupports *aCtxt, nsIInputStream **_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP
nsMultiMixedConv::AsyncConvertData(const PRUnichar *aFromType, const PRUnichar *aToType,
                                   nsIStreamListener *aListener, nsISupports *aCtxt) {
    NS_ASSERTION(aListener && aFromType && aToType, "null pointer passed into multi mixed converter");

    // hook up our final listener. this guy gets the various On*() calls we want to throw
    // at him.
    //
    // WARNING: this listener must be able to handle multiple OnStartRequest, OnDataAvail()
    //  and OnStopRequest() call combinations. We call of series of these for each sub-part
    //  in the raw stream.
    mFinalListener = aListener;
    return NS_OK;
}

#define ERR_OUT { nsMemory::Free(buffer); return rv; }

// nsIStreamListener implementation
NS_IMETHODIMP
nsMultiMixedConv::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {

    if (!mToken.get()) // no token, no love.
        return NS_ERROR_FAILURE;

    nsresult rv = NS_OK;
    char *buffer = nsnull;
    PRUint32 bufLen = count, read;

    NS_ASSERTION(request, "multimixed converter needs a request");

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    buffer = (char*)nsMemory::Alloc(bufLen);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    rv = inStr->Read(buffer, bufLen, &read);
    if (NS_FAILED(rv) || read == 0) return rv;
    NS_ASSERTION(read == bufLen, "poor data size assumption");
    
    if (mBufLen) {
		// incorporate any buffered data into the parsing
        char *tmp = (char*)nsMemory::Alloc(mBufLen + bufLen);
        if (!tmp) {
            nsMemory::Free(buffer);
            return NS_ERROR_OUT_OF_MEMORY;
        }
        nsCRT::memcpy(tmp, mBuffer, mBufLen);
        nsMemory::Free(mBuffer);
        mBuffer = nsnull;
        nsCRT::memcpy(tmp+mBufLen, buffer, bufLen);
        nsMemory::Free(buffer);
        buffer = tmp;
        bufLen += mBufLen;
        mBufLen = 0;
    }

    char *cursor = buffer;

    if (mFirstOnData) {
        // this is the first OnData() for this request. some servers
        // don't bother sending a token in the first "part." This is
        // illegal, but we'll handle the case anyway by shoving the
        // boundary token in for the server.
        mFirstOnData = PR_FALSE;
        NS_ASSERTION(!mBufLen, "this is our first time through, we can't have buffered data");
        const char * token = mToken;
           
        PushOverLine(cursor, bufLen);

        if (bufLen < mTokenLen+2) {
            // we don't have enough data yet to make this comparison.
            // skip this check, and try again the next time OnData()
            // is called.
            mFirstOnData = PR_TRUE;
        } else if (!PL_strnstr(cursor, token, mTokenLen+2)) {
            bufLen = read + mTokenLen + 1;
            char *tmp = (char*)nsMemory::Alloc(bufLen);
            if (!tmp) {
                nsMemory::Free(buffer);
                return NS_ERROR_OUT_OF_MEMORY;
            }
            nsCRT::memcpy(tmp, token, mTokenLen);
            nsCRT::memcpy(tmp+mTokenLen, "\n", 1);
            nsCRT::memcpy(tmp+mTokenLen+1, cursor, read);
            nsMemory::Free(buffer);
            buffer = tmp;
        }
    }

    char *token = nsnull;

    if (mProcessingHeaders) {
        // we were not able to process all the headers
        // for this "part" given the previous buffer given to 
        // us in the previous OnDataAvailable callback.
        PRBool done = PR_FALSE;
		rv = ParseHeaders(channel, cursor, bufLen, &done);
		if (NS_FAILED(rv)) ERR_OUT

        if (done) {
            mProcessingHeaders = PR_FALSE;
            rv = SendStart(channel);
            if (NS_FAILED(rv)) ERR_OUT
        }
    }

    PRInt32 tokenLinefeed = 1;
    while ( (token = FindToken(cursor, bufLen)) ) {

        if (*(token+mTokenLen+1) == '-') {
			// This was the last delimiter so we can stop processing
            bufLen = token - cursor;
            rv = SendData(cursor, bufLen);
            nsMemory::Free(buffer);
            if (NS_FAILED(rv)) return rv;
            buffer = nsnull;
            bufLen = 0;
            return SendStop(NS_OK);
        }

        if (!mNewPart && token > cursor) {
			// headers are processed, we're pushing data now.
			NS_ASSERTION(!mProcessingHeaders, "we should be pushing raw data");
            rv = SendData(cursor, token - cursor);
            bufLen -= token - cursor;
            if (NS_FAILED(rv)) ERR_OUT
        }
        token += mTokenLen;
        bufLen -= mTokenLen;
        tokenLinefeed = PushOverLine(token, bufLen);

        if (mNewPart) {
            // parse headers
            mNewPart = PR_FALSE;
            cursor = token;
            PRBool done = PR_FALSE; 
            rv = ParseHeaders(channel, cursor, bufLen, &done);
            if (NS_FAILED(rv)) ERR_OUT
            if (done) {
                rv = SendStart(channel);
                if (NS_FAILED(rv)) ERR_OUT
            } else {
                // we haven't finished processing header info.
                // we'll break out and try to process later.
                mProcessingHeaders = PR_TRUE;
                break;
            }
        } else {
            mNewPart = PR_TRUE;
            rv = SendStop(NS_OK);
            if (NS_FAILED(rv)) ERR_OUT
            // reset to the token to front.
            // this allows us to treat the token
            // as a starting token.
            token -= mTokenLen + tokenLinefeed;
            bufLen += mTokenLen + tokenLinefeed;
            cursor = token;
        }
    }

    // at this point, we want to buffer up whatever amount (bufLen)
    // we have leftover. However, we *always* want to ensure that
    // we buffer enough data to handle a broken token.

    // carry over
    PRUint32 bufAmt = 0;
    if (mProcessingHeaders) {
        bufAmt = bufLen;
    } else {
        // if the data ends in a linefeed, and we're in the middle
        // of a "part" (ie. mPartChannel exists) don't bother
        // buffering, go ahead and send the data we have. Otherwise
        // if we don't have a channel already, then we don't even
        // have enough info to start a part, go ahead and buffer
        // enough to collect a boundary token.
        if (!mPartChannel || !(cursor[bufLen-1] == nsCRT::LF) )
            bufAmt = PR_MIN(mTokenLen - 1, bufLen);
    }

    if (bufAmt) {
        rv = BufferData(cursor + (bufLen - bufAmt), bufAmt);
        if (NS_FAILED(rv)) ERR_OUT
        bufLen -= bufAmt;
    }

    if (bufLen) {
        rv = SendData(cursor, bufLen);
        if (NS_FAILED(rv)) ERR_OUT
    }

    nsMemory::Free(buffer);

    return rv;
}


// nsIRequestObserver implementation
NS_IMETHODIMP
nsMultiMixedConv::OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
	// we're assuming the content-type is available at this stage
	NS_ASSERTION(!mToken, "a second on start???");
    char *bndry = nsnull;
    nsXPIDLCString delimiter;
    nsresult rv = NS_OK;
    mContext = ctxt;

    mFirstOnData = PR_TRUE;
    mTotalSent   = 0;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    // ask the HTTP channel for the content-type and extract the boundary from it.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = httpChannel->GetResponseHeader("content-type", getter_Copies(delimiter));
        if (NS_FAILED(rv)) return rv;
    } else {
        // try asking the channel directly
        rv = channel->GetContentType(getter_Copies(delimiter));
        if (!delimiter || NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }

    if (!delimiter) return NS_ERROR_FAILURE;
    bndry = PL_strstr(delimiter, "boundary");
    if (!bndry) return NS_ERROR_FAILURE;

    bndry = PL_strchr(bndry, '=');
    if (!bndry) return NS_ERROR_FAILURE;

    bndry++; // move past the equals sign

    char *attrib = PL_strchr(bndry, ';');
    if (attrib) *attrib = '\0';

    nsCAutoString boundaryString(bndry);
    if (attrib) *attrib = ';';

    boundaryString.Trim(" \"");

    mToken.Adopt(nsCRT::strdup(boundaryString.get()));
    mTokenLen = boundaryString.Length();
    
    if (mTokenLen == 0 || !boundaryString.get())
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsMultiMixedConv::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                nsresult aStatus) {

    if (!mToken.get())  // no token, no love.
        return NS_ERROR_FAILURE;

    if (mPartChannel) {
        // we've already called SendStart() (which sets up the mPartChannel,
        // and fires an OnStart()) send any data left over, and then fire the stop.
        if (mBufLen > 0 && mBuffer) {
            (void) SendData(mBuffer, mBufLen);
            // don't bother checking the return value here, if the send failed
            // we're done anyway as we're in the OnStop() callback.
            nsMemory::Free(mBuffer);
            mBuffer = nsnull;
            mBufLen = 0;
        }
        (void) SendStop(aStatus);
    } else if (NS_FAILED(aStatus)) {
        // underlying data production problem. we should not be in
        // the middle of sending data. if we were, mPartChannel,
        // above, would have been true.
        
        // if we send the start, the URI Loader's m_targetStreamListener, may
        // be pointing at us causing a nice stack overflow.  So, don't call 
        // OnStartRequest!  -  This breaks necko's semantecs. 
        //(void) mFinalListener->OnStartRequest(request, ctxt);
        
        (void) mFinalListener->OnStopRequest(request, ctxt, aStatus);
    }

    return NS_OK;
}


// nsMultiMixedConv methods
nsMultiMixedConv::nsMultiMixedConv() {
    NS_INIT_ISUPPORTS();
    mTokenLen           = 0;
    mNewPart            = PR_TRUE;
    mContentLength      = -1;
    mBuffer             = nsnull;
    mBufLen             = 0;
    mProcessingHeaders  = PR_FALSE;
    mByteRangeStart     = 0;
    mByteRangeEnd       = 0;
    mTotalSent          = 0;
    mIsByteRangeRequest = PR_FALSE;
}

nsMultiMixedConv::~nsMultiMixedConv() {
    NS_ASSERTION(!mBuffer, "all buffered data should be gone");
    if (mBuffer) {
        nsMemory::Free(mBuffer);
        mBuffer = nsnull;
    }
}

nsresult
nsMultiMixedConv::BufferData(char *aData, PRUint32 aLen) {
    NS_ASSERTION(!mBuffer, "trying to over-write buffer");

    char *buffer = (char*)nsMemory::Alloc(aLen);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    nsCRT::memcpy(buffer, aData, aLen);
    mBuffer = buffer;
    mBufLen = aLen;
    return NS_OK;
}


nsresult
nsMultiMixedConv::SendStart(nsIChannel *aChannel) {
    nsresult rv = NS_OK;

    if (mContentType.IsEmpty())
        mContentType = UNKNOWN_CONTENT_TYPE;

    // if we already have an mPartChannel, that means we never sent a Stop()
    // before starting up another "part." that would be bad.
    NS_ASSERTION(!mPartChannel, "tisk tisk, shouldn't be overwriting a channel");

    nsPartChannel *newChannel;
    newChannel = new nsPartChannel(aChannel);
    if (!newChannel)
        return NS_ERROR_OUT_OF_MEMORY;

    if (mIsByteRangeRequest) {
        newChannel->InitializeByteRange(mByteRangeStart, mByteRangeEnd);
    }

    mTotalSent = 0;

    // Set up the new part channel...
    mPartChannel = newChannel;

    rv = mPartChannel->SetContentType(mContentType);
    if (NS_FAILED(rv)) return rv;

    mPartChannel->SetContentLength(mContentLength);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = 0;
    mPartChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_REPLACE;
    mPartChannel->SetLoadFlags(loadFlags);

    nsCOMPtr<nsILoadGroup> loadGroup;
    (void)mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));

    // Add the new channel to the load group (if any)
    if (loadGroup) {
        rv = loadGroup->AddRequest(mPartChannel, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    // Let's start off the load. NOTE: we don't forward on the channel passed
    // into our OnDataAvailable() as it's the root channel for the raw stream.
    return mFinalListener->OnStartRequest(mPartChannel, mContext);
}


nsresult
nsMultiMixedConv::SendStop(nsresult aStatus) {
    
    nsresult rv = NS_OK;
    if (mPartChannel) {
        rv = mFinalListener->OnStopRequest(mPartChannel, mContext, aStatus);
        // don't check for failure here, we need to remove the channel from 
        // the loadgroup.

        // Remove the channel from its load group (if any)
        nsCOMPtr<nsILoadGroup> loadGroup;
        (void) mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));
        if (loadGroup) 
            (void) loadGroup->RemoveRequest(mPartChannel, mContext, aStatus);
    }

    mPartChannel = 0;
    return rv;
}

nsresult
nsMultiMixedConv::SendData(char *aBuffer, PRUint32 aLen) {

    nsresult rv = NS_OK;
    
    if (!mPartChannel) return NS_ERROR_FAILURE; // something went wrong w/ processing


    if (mContentLength != -1) {
        // make sure that we don't send more than the mContentLength
        if ((aLen + mTotalSent) > mContentLength)
            aLen = mContentLength - mTotalSent;

        if (aLen == 0)
            return NS_OK;
    }

    mTotalSent += aLen;

    char *tmp = (char*)nsMemory::Alloc(aLen); // byteArray stream owns this mem
    if (!tmp) return NS_ERROR_OUT_OF_MEMORY;
    
    nsCRT::memcpy(tmp, aBuffer, aLen);
    
    nsCOMPtr<nsIByteArrayInputStream> byteArrayStream;
    
    rv = NS_NewByteArrayInputStream(getter_AddRefs(byteArrayStream), tmp, aLen);
    if (NS_FAILED(rv)) {
        nsMemory::Free(tmp);
        return rv;
    }

    nsCOMPtr<nsIInputStream> inStream = do_QueryInterface(byteArrayStream, &rv);
    if (NS_FAILED(rv)) return rv;

    PRUint32 len;
    rv = inStream->Available(&len);
    if (NS_FAILED(rv)) return rv;

    return mFinalListener->OnDataAvailable(mPartChannel, mContext, inStream, 0, len);
}

PRInt32
nsMultiMixedConv::PushOverLine(char *&aPtr, PRUint32 &aLen) {
    PRInt32 chars = 0;
    if (*aPtr == nsCRT::CR || *aPtr == nsCRT::LF) {
        if (aPtr[1] == nsCRT::LF)
            chars++;
        chars++;
    }
    aPtr += chars;
    aLen -= chars;
    return chars;
}

nsresult
nsMultiMixedConv::ParseHeaders(nsIChannel *aChannel, char *&aPtr, 
                               PRUint32 &aLen, PRBool *_retval) {
    // NOTE: this data must be ascii.
	nsresult rv = NS_OK;
    char *cursor = aPtr, *newLine = nsnull;
    PRUint32 cursorLen = aLen;
    PRBool done = PR_FALSE;
    PRUint32 lineFeedIncrement = 1;
    
    mContentLength = -1;
    while ( (cursorLen > 0) 
            && (newLine = PL_strchr(cursor, nsCRT::LF)) ) {
        // adjust for linefeeds
        if ( (newLine > cursor) && (newLine[-1] == nsCRT::CR) ) { // CRLF
             lineFeedIncrement = 2;
             newLine--;
        }

        if (newLine == cursor) {
            // move the newLine beyond the double linefeed marker
            if ( (newLine - cursor) <= ((PRInt32)cursorLen - lineFeedIncrement) ) {
                newLine += lineFeedIncrement;
            }
            cursorLen -= (newLine - cursor);
            cursor = newLine;

            done = PR_TRUE;
            break;
        }

        char tmpChar = *newLine;
        *newLine = '\0';
        char *colon = PL_strchr(cursor, ':');
        if (colon) {
            *colon = '\0';
            nsCAutoString headerStr(cursor);
            headerStr.CompressWhitespace();
            *colon = ':';

            nsCAutoString headerVal(colon + 1);
            headerVal.CompressWhitespace();

            // examine header
            if (headerStr.EqualsIgnoreCase("content-type")) {
                char *tmpString = ToNewCString(headerVal);
                ParseContentType(tmpString);
                nsCRT::free(tmpString);
            } else if (headerStr.EqualsIgnoreCase("content-length")) {
                mContentLength = atoi(headerVal.get());
            } else if (headerStr.EqualsIgnoreCase("set-cookie")) {
                // setting headers on the HTTP channel
                // causes HTTP to notify, again if necessary,
                // it's header observers.
                nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
                if (httpChannel) {
                    rv = httpChannel->SetResponseHeader(headerStr, headerVal);
                    if (NS_FAILED(rv)) return rv;
                }
            } else if (headerStr.EqualsIgnoreCase("content-range") || 
                       headerStr.EqualsIgnoreCase("range") ) {
                // something like: Content-range: bytes 7000-7999/8000
                char* tmpPtr;

                tmpPtr = PL_strchr(colon + 1, '/');
                if (tmpPtr) 
                    *tmpPtr = '\0';

                // pass the bytes-unit and the SP
                char *range = PL_strchr(colon + 2, ' ');

                if (!range)
                    return NS_ERROR_FAILURE;

                if (range[0] == '*'){
                    mByteRangeStart = mByteRangeEnd = 0;
                }
                else {
                    tmpPtr = PL_strchr(range, '-');
                    if (!tmpPtr)
                        return NS_ERROR_FAILURE;
                    
                    tmpPtr[0] = '\0';
                    
                    mByteRangeStart = atoi(range);
                    tmpPtr++;
                    mByteRangeEnd = atoi(tmpPtr);
                }

                mIsByteRangeRequest = PR_TRUE;
                if (mContentLength == -1)   
                    mContentLength = mByteRangeEnd - mByteRangeStart + 1;
            }
        }
        *newLine = tmpChar;
        newLine += lineFeedIncrement;
        cursorLen -= (newLine - cursor);
        cursor = newLine;
    } // end while (newLine)

    aPtr = cursor;
    aLen = cursorLen;

	*_retval = done;
    return rv;
}

// This code is duplicated in nsHttpResponseHead.cpp.  If you change it
// here, change it there, too!

nsresult
nsMultiMixedConv::ParseContentType(char *type)
{
    // don't bother with an empty content-type header - bug 83465
    if (!*type)
        return NS_OK;

    // we don't care about comments (although they are invalid here)
    char *p = PL_strchr(type, '(');
    if (p)
        *p = 0;

    // check if the content-type has additional fields...
    if ((p = PL_strchr(type, ';')) != nsnull) {
        char *p2, *p3;
        // is there a charset field?
        if ((p2 = PL_strcasestr(p, "charset=")) != nsnull) {
            p2 += 8;

            // check end of charset parameter
            if ((p3 = PL_strchr(p2, ';')) == nsnull)
                p3 = p2 + PL_strlen(p2);

            // trim any trailing whitespace
            do {
                --p3;
            } while ((*p3 == ' ') || (*p3 == '\t'));
            *++p3 = 0; // overwrite first char after the charset field

            mContentCharset = p2;
        }
    }
    else
        p = type + PL_strlen(type);

    // trim any trailing whitespace
    while (--p >= type && ((*p == ' ') || (*p == '\t')))
        ;
    *++p = 0; // overwrite first char after the media type

    // force the content-type to lowercase
    while (--p >= type)
        *p = nsCRT::ToLower(*p);

    mContentType = type;

    return NS_OK;
}

char *
nsMultiMixedConv::FindToken(char *aCursor, PRUint32 aLen) {
    // strnstr without looking for null termination
    const char *token = mToken;
    char *cur = aCursor;

    NS_ASSERTION(token && aCursor && *token, "bad data");

    if( mTokenLen > aLen ) return nsnull;

    for( ; aLen >= mTokenLen; aCursor++, aLen-- )
        if( *token == *aCursor)
            if(!memcmp(aCursor, token, mTokenLen) ) {
                if ( (aCursor - cur) >= 2) {
                    // back the cursor up over a double dash for backwards compat.
                    if ( (*(aCursor-1) == '-') && (*(aCursor-2) == '-') ) {
                        aCursor -= 2;
                        aLen += 2;

                        // we're playing w/ double dash tokens, adjust.
                        nsCString newToken("--");
                        newToken.Append(mToken);
                        mToken.Adopt(nsCRT::strdup(newToken.get()));
                        mTokenLen += 2;
                    }
                }
                return aCursor;
            }

    return nsnull;
}

nsresult
NS_NewMultiMixedConv(nsMultiMixedConv** aMultiMixedConv)
{
    NS_PRECONDITION(aMultiMixedConv != nsnull, "null ptr");
    if (! aMultiMixedConv)
        return NS_ERROR_NULL_POINTER;

    *aMultiMixedConv = new nsMultiMixedConv();
    if (! *aMultiMixedConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aMultiMixedConv);
    return NS_OK;
}

