/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMultiMixedConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "nsIHttpChannel.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIStringStream.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsIHttpChannelInternal.h"
#include "nsURLHelper.h"
#include "nsIStreamConverterService.h"

//
// Helper function for determining the length of data bytes up to
// the next multipart token.  A token is usually preceded by a LF
// or CRLF delimiter.
// 
static PRUint32
LengthToToken(const char *cursor, const char *token)
{
    PRUint32 len = token - cursor;
    // Trim off any LF or CRLF preceding the token
    if (len && *(token-1) == '\n') {
        --len;
        if (len && *(token-2) == '\r')
            --len;
    }
    return len;
}

nsPartChannel::nsPartChannel(nsIChannel *aMultipartChannel, PRUint32 aPartID,
                             nsIStreamListener* aListener) :
  mMultipartChannel(aMultipartChannel),
  mListener(aListener),
  mStatus(NS_OK),
  mContentLength(LL_MAXUINT),
  mIsByteRangeRequest(false),
  mByteRangeStart(0),
  mByteRangeEnd(0),
  mPartID(aPartID),
  mIsLastPart(false)
{
    mMultipartChannel = aMultipartChannel;

    // Inherit the load flags from the original channel...
    mMultipartChannel->GetLoadFlags(&mLoadFlags);

    mMultipartChannel->GetLoadGroup(getter_AddRefs(mLoadGroup));
}

nsPartChannel::~nsPartChannel()
{
}

void nsPartChannel::InitializeByteRange(PRInt64 aStart, PRInt64 aEnd)
{
    mIsByteRangeRequest = true;
    
    mByteRangeStart = aStart;
    mByteRangeEnd   = aEnd;
}

nsresult nsPartChannel::SendOnStartRequest(nsISupports* aContext)
{
    return mListener->OnStartRequest(this, aContext);
}

nsresult nsPartChannel::SendOnDataAvailable(nsISupports* aContext,
                                            nsIInputStream* aStream,
                                            PRUint32 aOffset, PRUint32 aLen)
{
    return mListener->OnDataAvailable(this, aContext, aStream, aOffset, aLen);
}

nsresult nsPartChannel::SendOnStopRequest(nsISupports* aContext,
                                          nsresult aStatus)
{
    // Drop the listener
    nsCOMPtr<nsIStreamListener> listener;
    listener.swap(mListener);
    return listener->OnStopRequest(this, aContext, aStatus);
}

void nsPartChannel::SetContentDisposition(const nsACString& aContentDispositionHeader)
{
    mContentDispositionHeader = aContentDispositionHeader;
    nsCOMPtr<nsIURI> uri;
    GetURI(getter_AddRefs(uri));
    NS_GetFilenameFromDisposition(mContentDispositionFilename,
                                  mContentDispositionHeader, uri);
    mContentDisposition = NS_GetContentDispositionFromHeader(mContentDispositionHeader, this);
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
    NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannel)
NS_INTERFACE_MAP_END

//
// nsIRequest implementation...
//

NS_IMETHODIMP
nsPartChannel::GetName(nsACString &aResult)
{
    return mMultipartChannel->GetName(aResult);
}

NS_IMETHODIMP
nsPartChannel::IsPending(bool *aResult)
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
    // XXX but we should stop sending data for _this_ part channel!
    mStatus = aStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::Suspend(void)
{
    // Suspending an individual part must not suspend the underlying
    // multipart channel...
    // XXX why not?
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::Resume(void)
{
    // Resuming an individual part must not resume the underlying
    // multipart channel...
    // XXX why not?
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
nsPartChannel::GetContentType(nsACString &aContentType)
{
    aContentType = mContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentType(const nsACString &aContentType)
{
    bool dummy;
    net_ParseContentType(aContentType, mContentType, mContentCharset, &dummy);
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentCharset(nsACString &aContentCharset)
{
    aContentCharset = mContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentCharset(const nsACString &aContentCharset)
{
    mContentCharset = aContentCharset;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength; // XXX truncates 64-bit value
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentLength(PRInt32 aContentLength)
{
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentDisposition(PRUint32 *aContentDisposition)
{
    if (mContentDispositionHeader.IsEmpty())
        return NS_ERROR_NOT_AVAILABLE;

    *aContentDisposition = mContentDisposition;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
    if (mContentDispositionFilename.IsEmpty())
        return NS_ERROR_NOT_AVAILABLE;

    aContentDispositionFilename = mContentDispositionFilename;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
    if (mContentDispositionHeader.IsEmpty())
        return NS_ERROR_NOT_AVAILABLE;

    aContentDispositionHeader = mContentDispositionHeader;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetPartID(PRUint32 *aPartID)
{
    *aPartID = mPartID;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetIsLastPart(bool *aIsLastPart)
{
    *aIsLastPart = mIsLastPart;
    return NS_OK;
}

//
// nsIByteRangeRequest implementation...
//

NS_IMETHODIMP 
nsPartChannel::GetIsByteRangeRequest(bool *aIsByteRangeRequest)
{
    *aIsByteRangeRequest = mIsByteRangeRequest;

    return NS_OK;
}


NS_IMETHODIMP 
nsPartChannel::GetStartRange(PRInt64 *aStartRange)
{
    *aStartRange = mByteRangeStart;

    return NS_OK;
}

NS_IMETHODIMP 
nsPartChannel::GetEndRange(PRInt64 *aEndRange)
{
    *aEndRange = mByteRangeEnd;
    return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetBaseChannel(nsIChannel ** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    *aReturn = mMultipartChannel;
    NS_IF_ADDREF(*aReturn);
    return NS_OK;
}


// nsISupports implementation
NS_IMPL_ISUPPORTS3(nsMultiMixedConv,
                   nsIStreamConverter,
                   nsIStreamListener,
                   nsIRequestObserver)


// nsIStreamConverter implementation

// No syncronous conversion at this time.
NS_IMETHODIMP
nsMultiMixedConv::Convert(nsIInputStream *aFromStream,
                          const char *aFromType,
                          const char *aToType,
                          nsISupports *aCtxt, nsIInputStream **_retval) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter (us).
NS_IMETHODIMP
nsMultiMixedConv::AsyncConvertData(const char *aFromType, const char *aToType,
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

// AutoFree implementation to prevent memory leaks
class AutoFree
{
public:
  AutoFree() : mBuffer(NULL) {}

  AutoFree(char *buffer) : mBuffer(buffer) {}

  ~AutoFree() {
    free(mBuffer);
  }

  AutoFree& operator=(char *buffer) {
    mBuffer = buffer;
    return *this;
  }

  operator char*() const {
    return mBuffer;
  }
private:
  char *mBuffer;
};

// nsIStreamListener implementation
NS_IMETHODIMP
nsMultiMixedConv::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {

    if (mToken.IsEmpty()) // no token, no love.
        return NS_ERROR_FAILURE;

    nsresult rv = NS_OK;
    AutoFree buffer = nullptr;
    PRUint32 bufLen = 0, read = 0;

    NS_ASSERTION(request, "multimixed converter needs a request");

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;

    // fill buffer
    {
        bufLen = count + mBufLen;
        NS_ENSURE_TRUE((bufLen >= count) && (bufLen >= mBufLen),
                       NS_ERROR_FAILURE);
        buffer = (char *) malloc(bufLen);
        if (!buffer)
            return NS_ERROR_OUT_OF_MEMORY;

        if (mBufLen) {
            // incorporate any buffered data into the parsing
            memcpy(buffer, mBuffer, mBufLen);
            free(mBuffer);
            mBuffer = 0;
            mBufLen = 0;
        }
        
        rv = inStr->Read(buffer + (bufLen - count), count, &read);

        if (NS_FAILED(rv) || read == 0) return rv;
        NS_ASSERTION(read == count, "poor data size assumption");
    }

    char *cursor = buffer;

    if (mFirstOnData) {
        // this is the first OnData() for this request. some servers
        // don't bother sending a token in the first "part." This is
        // illegal, but we'll handle the case anyway by shoving the
        // boundary token in for the server.
        mFirstOnData = false;
        NS_ASSERTION(!mBufLen, "this is our first time through, we can't have buffered data");
        const char * token = mToken.get();
           
        PushOverLine(cursor, bufLen);

        if (bufLen < mTokenLen+2) {
            // we don't have enough data yet to make this comparison.
            // skip this check, and try again the next time OnData()
            // is called.
            mFirstOnData = true;
        }
        else if (!PL_strnstr(cursor, token, mTokenLen+2)) {
            buffer = (char *) realloc(buffer, bufLen + mTokenLen + 1);
            if (!buffer)
                return NS_ERROR_OUT_OF_MEMORY;

            memmove(buffer + mTokenLen + 1, buffer, bufLen);
            memcpy(buffer, token, mTokenLen);
            buffer[mTokenLen] = '\n';

            bufLen += (mTokenLen + 1);

            // need to reset cursor to the buffer again (bug 100595)
            cursor = buffer;
        }
    }

    char *token = nullptr;

    if (mProcessingHeaders) {
        // we were not able to process all the headers
        // for this "part" given the previous buffer given to 
        // us in the previous OnDataAvailable callback.
        bool done = false;
        rv = ParseHeaders(channel, cursor, bufLen, &done);
        if (NS_FAILED(rv)) return rv;

        if (done) {
            mProcessingHeaders = false;
            rv = SendStart(channel);
            if (NS_FAILED(rv)) return rv;
        }
    }

    PRInt32 tokenLinefeed = 1;
    while ( (token = FindToken(cursor, bufLen)) ) {

        if (*(token+mTokenLen+1) == '-') {
            // This was the last delimiter so we can stop processing
            rv = SendData(cursor, LengthToToken(cursor, token));
            if (NS_FAILED(rv)) return rv;
            return SendStop(NS_OK);
        }

        if (!mNewPart && token > cursor) {
            // headers are processed, we're pushing data now.
            NS_ASSERTION(!mProcessingHeaders, "we should be pushing raw data");
            rv = SendData(cursor, LengthToToken(cursor, token));
            bufLen -= token - cursor;
            if (NS_FAILED(rv)) return rv;
        }
        // XXX else NS_ASSERTION(token == cursor, "?");
        token += mTokenLen;
        bufLen -= mTokenLen;
        tokenLinefeed = PushOverLine(token, bufLen);

        if (mNewPart) {
            // parse headers
            mNewPart = false;
            cursor = token;
            bool done = false; 
            rv = ParseHeaders(channel, cursor, bufLen, &done);
            if (NS_FAILED(rv)) return rv;
            if (done) {
                rv = SendStart(channel);
                if (NS_FAILED(rv)) return rv;
            }
            else {
                // we haven't finished processing header info.
                // we'll break out and try to process later.
                mProcessingHeaders = true;
                break;
            }
        }
        else {
            mNewPart = true;
            // Reset state so we don't carry it over from part to part
            mContentType.Truncate();
            mContentLength = LL_MAXUINT;
            mContentDisposition.Truncate();
            mIsByteRangeRequest = false;
            mByteRangeStart = 0;
            mByteRangeEnd = 0;
            
            rv = SendStop(NS_OK);
            if (NS_FAILED(rv)) return rv;
            // reset the token to front. this allows us to treat
            // the token as a starting token.
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
    if (mProcessingHeaders)
        bufAmt = bufLen;
    else if (bufLen) {
        // if the data ends in a linefeed, and we're in the middle
        // of a "part" (ie. mPartChannel exists) don't bother
        // buffering, go ahead and send the data we have. Otherwise
        // if we don't have a channel already, then we don't even
        // have enough info to start a part, go ahead and buffer
        // enough to collect a boundary token.
        if (!mPartChannel || !(cursor[bufLen-1] == nsCRT::LF) )
            bufAmt = NS_MIN(mTokenLen - 1, bufLen);
    }

    if (bufAmt) {
        rv = BufferData(cursor + (bufLen - bufAmt), bufAmt);
        if (NS_FAILED(rv)) return rv;
        bufLen -= bufAmt;
    }

    if (bufLen) {
        rv = SendData(cursor, bufLen);
        if (NS_FAILED(rv)) return rv;
    }

    return rv;
}


// nsIRequestObserver implementation
NS_IMETHODIMP
nsMultiMixedConv::OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
    // we're assuming the content-type is available at this stage
    NS_ASSERTION(mToken.IsEmpty(), "a second on start???");
    const char *bndry = nullptr;
    nsCAutoString delimiter;
    nsresult rv = NS_OK;
    mContext = ctxt;

    mFirstOnData = true;
    mTotalSent   = 0;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    // ask the HTTP channel for the content-type and extract the boundary from it.
    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(channel, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("content-type"), delimiter);
        if (NS_FAILED(rv)) return rv;
    } else {
        // try asking the channel directly
        rv = channel->GetContentType(delimiter);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    }

    bndry = strstr(delimiter.BeginWriting(), "boundary");
    if (!bndry) return NS_ERROR_FAILURE;

    bndry = strchr(bndry, '=');
    if (!bndry) return NS_ERROR_FAILURE;

    bndry++; // move past the equals sign

    char *attrib = (char *) strchr(bndry, ';');
    if (attrib) *attrib = '\0';

    nsCAutoString boundaryString(bndry);
    if (attrib) *attrib = ';';

    boundaryString.Trim(" \"");

    mToken = boundaryString;
    mTokenLen = boundaryString.Length();
    
    if (mTokenLen == 0)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsMultiMixedConv::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                nsresult aStatus) {

    if (mToken.IsEmpty())  // no token, no love.
        return NS_ERROR_FAILURE;

    if (mPartChannel) {
        mPartChannel->SetIsLastPart();

        // we've already called SendStart() (which sets up the mPartChannel,
        // and fires an OnStart()) send any data left over, and then fire the stop.
        if (mBufLen > 0 && mBuffer) {
            (void) SendData(mBuffer, mBufLen);
            // don't bother checking the return value here, if the send failed
            // we're done anyway as we're in the OnStop() callback.
            free(mBuffer);
            mBuffer = nullptr;
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
nsMultiMixedConv::nsMultiMixedConv() :
  mCurrentPartID(0)
{
    mTokenLen           = 0;
    mNewPart            = true;
    mContentLength      = LL_MAXUINT;
    mBuffer             = nullptr;
    mBufLen             = 0;
    mProcessingHeaders  = false;
    mByteRangeStart     = 0;
    mByteRangeEnd       = 0;
    mTotalSent          = 0;
    mIsByteRangeRequest = false;
}

nsMultiMixedConv::~nsMultiMixedConv() {
    NS_ASSERTION(!mBuffer, "all buffered data should be gone");
    if (mBuffer) {
        free(mBuffer);
        mBuffer = nullptr;
    }
}

nsresult
nsMultiMixedConv::BufferData(char *aData, PRUint32 aLen) {
    NS_ASSERTION(!mBuffer, "trying to over-write buffer");

    char *buffer = (char *) malloc(aLen);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    memcpy(buffer, aData, aLen);
    mBuffer = buffer;
    mBufLen = aLen;
    return NS_OK;
}


nsresult
nsMultiMixedConv::SendStart(nsIChannel *aChannel) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIStreamListener> partListener(mFinalListener);
    if (mContentType.IsEmpty()) {
        mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
        nsCOMPtr<nsIStreamConverterService> serv =
            do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIStreamListener> converter;
            rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE,
                                        "*/*",
                                        mFinalListener,
                                        mContext,
                                        getter_AddRefs(converter));
            if (NS_SUCCEEDED(rv)) {
                partListener = converter;
            }
        }
    }

    // if we already have an mPartChannel, that means we never sent a Stop()
    // before starting up another "part." that would be bad.
    NS_ASSERTION(!mPartChannel, "tisk tisk, shouldn't be overwriting a channel");

    nsPartChannel *newChannel;
    newChannel = new nsPartChannel(aChannel, mCurrentPartID++, partListener);
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

    rv = mPartChannel->SetContentLength(mContentLength); // XXX Truncates 64-bit!
    if (NS_FAILED(rv)) return rv;

    mPartChannel->SetContentDisposition(mContentDisposition);

    nsLoadFlags loadFlags = 0;
    mPartChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsIChannel::LOAD_REPLACE;
    mPartChannel->SetLoadFlags(loadFlags);

    nsCOMPtr<nsILoadGroup> loadGroup;
    (void)mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));

    // Add the new channel to the load group (if any)
    if (loadGroup) {
        rv = loadGroup->AddRequest(mPartChannel, nullptr);
        if (NS_FAILED(rv)) return rv;
    }

    // Let's start off the load. NOTE: we don't forward on the channel passed
    // into our OnDataAvailable() as it's the root channel for the raw stream.
    return mPartChannel->SendOnStartRequest(mContext);
}


nsresult
nsMultiMixedConv::SendStop(nsresult aStatus) {
    
    nsresult rv = NS_OK;
    if (mPartChannel) {
        rv = mPartChannel->SendOnStopRequest(mContext, aStatus);
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

    if (mContentLength != LL_MAXUINT) {
        // make sure that we don't send more than the mContentLength
        // XXX why? perhaps the Content-Length header was actually wrong!!
        if ((PRUint64(aLen) + mTotalSent) > mContentLength)
            aLen = mContentLength - mTotalSent;

        if (aLen == 0)
            return NS_OK;
    }

    PRUint32 offset = mTotalSent;
    mTotalSent += aLen;

    nsCOMPtr<nsIStringInputStream> ss(
            do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv));
    if (NS_FAILED(rv))
        return rv;

    rv = ss->ShareData(aBuffer, aLen);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIInputStream> inStream(do_QueryInterface(ss, &rv));
    if (NS_FAILED(rv)) return rv;

    return mPartChannel->SendOnDataAvailable(mContext, inStream, offset, aLen);
}

PRInt32
nsMultiMixedConv::PushOverLine(char *&aPtr, PRUint32 &aLen) {
    PRInt32 chars = 0;
    if ((aLen > 0) && (*aPtr == nsCRT::CR || *aPtr == nsCRT::LF)) {
        if ((aLen > 1) && (aPtr[1] == nsCRT::LF))
            chars++;
        chars++;
        aPtr += chars;
        aLen -= chars;
    }
    return chars;
}

nsresult
nsMultiMixedConv::ParseHeaders(nsIChannel *aChannel, char *&aPtr, 
                               PRUint32 &aLen, bool *_retval) {
    // NOTE: this data must be ascii.
    // NOTE: aPtr is NOT null terminated!
    nsresult rv = NS_OK;
    char *cursor = aPtr, *newLine = nullptr;
    PRUint32 cursorLen = aLen;
    bool done = false;
    PRUint32 lineFeedIncrement = 1;
    
    mContentLength = LL_MAXUINT; // XXX what if we were already called?
    while (cursorLen && (newLine = (char *) memchr(cursor, nsCRT::LF, cursorLen))) {
        // adjust for linefeeds
        if ((newLine > cursor) && (newLine[-1] == nsCRT::CR) ) { // CRLF
            lineFeedIncrement = 2;
            newLine--;
        }
        else
            lineFeedIncrement = 1; // reset

        if (newLine == cursor) {
            // move the newLine beyond the linefeed marker
            NS_ASSERTION(cursorLen >= lineFeedIncrement, "oops!");

            cursor += lineFeedIncrement;
            cursorLen -= lineFeedIncrement;

            done = true;
            break;
        }

        char tmpChar = *newLine;
        *newLine = '\0'; // cursor is now null terminated
        char *colon = (char *) strchr(cursor, ':');
        if (colon) {
            *colon = '\0';
            nsCAutoString headerStr(cursor);
            headerStr.CompressWhitespace();
            *colon = ':';

            nsCAutoString headerVal(colon + 1);
            headerVal.CompressWhitespace();

            // examine header
            if (headerStr.LowerCaseEqualsLiteral("content-type")) {
                mContentType = headerVal;
            } else if (headerStr.LowerCaseEqualsLiteral("content-length")) {
                mContentLength = atoi(headerVal.get()); // XXX 64-bit math?
            } else if (headerStr.LowerCaseEqualsLiteral("content-disposition")) {
                mContentDisposition = headerVal;
            } else if (headerStr.LowerCaseEqualsLiteral("set-cookie")) {
                nsCOMPtr<nsIHttpChannelInternal> httpInternal =
                    do_QueryInterface(aChannel);
                if (httpInternal) {
                    httpInternal->SetCookie(headerVal.get());
                }
            } else if (headerStr.LowerCaseEqualsLiteral("content-range") || 
                       headerStr.LowerCaseEqualsLiteral("range") ) {
                // something like: Content-range: bytes 7000-7999/8000
                char* tmpPtr;

                tmpPtr = (char *) strchr(colon + 1, '/');
                if (tmpPtr) 
                    *tmpPtr = '\0';

                // pass the bytes-unit and the SP
                char *range = (char *) strchr(colon + 2, ' ');

                if (!range)
                    return NS_ERROR_FAILURE;

                if (range[0] == '*'){
                    mByteRangeStart = mByteRangeEnd = 0;
                }
                else {
                    tmpPtr = (char *) strchr(range, '-');
                    if (!tmpPtr)
                        return NS_ERROR_FAILURE;
                    
                    tmpPtr[0] = '\0';
                    
                    mByteRangeStart = atoi(range); // XXX want 64-bit conv
                    tmpPtr++;
                    mByteRangeEnd = atoi(tmpPtr);
                }

                mIsByteRangeRequest = true;
                if (mContentLength == LL_MAXUINT)
                    mContentLength = PRUint64(PRInt64(mByteRangeEnd - mByteRangeStart + PRInt64(1)));
            }
        }
        *newLine = tmpChar;
        newLine += lineFeedIncrement;
        cursorLen -= (newLine - cursor);
        cursor = newLine;
    }

    aPtr = cursor;
    aLen = cursorLen;

    *_retval = done;
    return rv;
}

char *
nsMultiMixedConv::FindToken(char *aCursor, PRUint32 aLen) {
    // strnstr without looking for null termination
    const char *token = mToken.get();
    char *cur = aCursor;

    if (!(token && aCursor && *token)) {
        NS_WARNING("bad data");
        return nullptr;
    }

    for (; aLen >= mTokenLen; aCursor++, aLen--) {
        if (!memcmp(aCursor, token, mTokenLen) ) {
            if ((aCursor - cur) >= 2) {
                // back the cursor up over a double dash for backwards compat.
                if ((*(aCursor-1) == '-') && (*(aCursor-2) == '-')) {
                    aCursor -= 2;
                    aLen += 2;

                    // we're playing w/ double dash tokens, adjust.
                    mToken.Assign(aCursor, mTokenLen + 2);
                    mTokenLen = mToken.Length();
                }
            }
            return aCursor;
        }
    }

    return nullptr;
}

nsresult
NS_NewMultiMixedConv(nsMultiMixedConv** aMultiMixedConv)
{
    NS_PRECONDITION(aMultiMixedConv != nullptr, "null ptr");
    if (! aMultiMixedConv)
        return NS_ERROR_NULL_POINTER;

    *aMultiMixedConv = new nsMultiMixedConv();
    if (! *aMultiMixedConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aMultiMixedConv);
    return NS_OK;
}

