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

#include "nsMultiMixedConv.h"
#include "nsMemory.h"
#include "plstr.h"
#include "nsIHTTPChannel.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIByteArrayInputStream.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

// nsISupports implementation
NS_IMPL_THREADSAFE_ISUPPORTS3(nsMultiMixedConv,
                              nsIStreamConverter, 
							  nsIStreamListener,
                              nsIStreamObserver);


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
    nsresult rv = NS_OK;
    char *buffer = nsnull;
    PRUint32 bufLen = count, read;

    NS_ASSERTION(request, "multimixed converter needs a request");

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    buffer = (char*)nsMemory::Alloc(bufLen);
    if (!buffer) ERR_OUT

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

    char *cursor = buffer, *token = nsnull;

    if (mProcessingHeaders) {
        PRBool done = PR_FALSE;
		rv = ParseHeaders(channel, cursor, bufLen, &done);
		if (NS_FAILED(rv)) ERR_OUT

        if (done) {
            mProcessingHeaders = PR_FALSE;
            rv = SendStart(channel);
            if (NS_FAILED(rv)) ERR_OUT
        }
    }

    PRInt8 tokenLinefeed = 1;
    while ( (token = FindToken(cursor, bufLen)) ) {

        if (*(token+mTokenLen+1) == '-') {
			// This was the last delimiter so we can stop processing
            bufLen = token - cursor;
            rv = SendData(cursor, bufLen);
            nsMemory::Free(buffer);
            buffer = nsnull;
            bufLen = 0;
            return SendStop();
        }

        if (!mNewPart && token > cursor) {
			// headers are processed, we're pushing data now.
			NS_ASSERTION(!mProcessingHeaders, "we should be pushing raw data");
            NS_ASSERTION(mPartChannel, "implies we're processing");
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
            rv = SendStop();
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
        // if the data ends in a linefeed, we
        // know the token can't cross it, thus
        // we know there's no token left - don't buffer
        if (!(cursor[bufLen-1] == LF))
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


// nsIStreamObserver implementation
NS_IMETHODIMP
nsMultiMixedConv::OnStartRequest(nsIRequest *request, nsISupports *ctxt) {
	// we're assuming the content-type is available at this stage
	NS_ASSERTION(!mToken, "a second on start???");
    char *bndry = nsnull;
    nsXPIDLCString delimiter;
    nsresult rv = NS_OK;
    mContext = ctxt;

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(request, &rv);
    if (NS_FAILED(rv)) return rv;
    
    // ask the HTTP channel for the content-type and extract the boundary from it.
    nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIAtom> header = NS_NewAtom("content-type");
        if (!header) return NS_ERROR_OUT_OF_MEMORY;
        rv = httpChannel->GetResponseHeader(header, getter_Copies(delimiter));
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

    mToken = boundaryString.GetBuffer();
    if (!mToken) return NS_ERROR_OUT_OF_MEMORY;
    mTokenLen = boundaryString.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsMultiMixedConv::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                nsresult aStatus, const PRUnichar* aStatusArg) {
    nsresult rv = NS_OK;

    if (NS_FAILED(aStatus)) {
        if (mPartChannel) {
            // we've already fired an onstart.
            // push any buffered data out and then push
            // an onstop out.
            if (mBufLen > 0) {
                rv = SendData(mBuffer, mBufLen);
                if (NS_FAILED(rv)) return rv;
                nsMemory::Free(mBuffer);
                mBuffer = nsnull;
                mBufLen = 0;
            }
            rv = mFinalListener->OnStopRequest(mPartChannel, mContext, aStatus, aStatusArg);
        } else {
            rv = mFinalListener->OnStartRequest(request, ctxt);
            if (NS_FAILED(rv)) return rv;

            rv = mFinalListener->OnStopRequest(request, ctxt, aStatus, aStatusArg);
        }
    }
    else {
        if (mBufLen > 0 && mBuffer)
        {
            SendData(mBuffer, mBufLen);
            nsMemory::Free(mBuffer);
            mBuffer = nsnull;
            mBufLen = 0;
            rv = SendStop ();
        }
    }
    return rv;
}


// nsMultiMixedConv methods
nsMultiMixedConv::nsMultiMixedConv() {
    NS_INIT_ISUPPORTS();
    mTokenLen           = mPartCount = 0;
    mNewPart            = PR_TRUE;
    mContentLength      = -1;
    mBuffer             = nsnull;
    mBufLen             = 0;
    mProcessingHeaders  = PR_FALSE;
    mEndingNewline      = PR_FALSE;
}

nsMultiMixedConv::~nsMultiMixedConv() {
    NS_ASSERTION(!mBuffer, "all buffered data should be gone");
}

nsresult
nsMultiMixedConv::Init() {
	nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIIOService, ioService, kIOServiceCID, &rv);
    mIOService = ioService;
    return rv;
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

    // First build up a dummy uri.
    nsCOMPtr<nsIURI> partURI;
    rv = aChannel->GetURI(getter_AddRefs(partURI));
    if (NS_FAILED(rv)) return rv;

    if (mContentType.IsEmpty())
        mContentType = UNKNOWN_CONTENT_TYPE;

    rv = NS_NewInputStreamChannel(getter_AddRefs(mPartChannel), 
                                                 partURI, 
                                                 nsnull,       // inStr
                                                 mContentType.GetBuffer(), 
                                                 mContentLength);
    if (NS_FAILED(rv)) return rv;

    nsLoadFlags loadFlags = 0;
    mPartChannel->GetLoadAttributes(&loadFlags);
    loadFlags |= nsIChannel::LOAD_REPLACE;
    mPartChannel->SetLoadAttributes(loadFlags);

	nsCOMPtr<nsILoadGroup> loadGroup;
    (void)aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    // Add the new channel to the load group (if any)
    if (loadGroup) {
        rv = mPartChannel->SetLoadGroup(loadGroup);
        if (NS_FAILED(rv)) return rv;
        rv = loadGroup->AddRequest(mPartChannel, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    // Let's start off the load. NOTE: we don't forward on the channel passed
    // into our OnDataAvailable() as it's the root channel for the raw stream.
    return mFinalListener->OnStartRequest(mPartChannel, mContext);
}


nsresult
nsMultiMixedConv::SendStop() {
    nsresult rv = mFinalListener->OnStopRequest(mPartChannel, mContext, NS_OK, nsnull);
    if (NS_FAILED(rv)) return rv;

    // Remove the channel from its load group (if any)
    nsCOMPtr<nsILoadGroup> loadGroup;

    (void) mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
        loadGroup->RemoveRequest(mPartChannel, mContext, NS_OK, nsnull);
    }

    mPartChannel = 0;
    return rv;
}

nsresult
nsMultiMixedConv::SendData(char *aBuffer, PRUint32 aLen) {

    nsresult rv = NS_OK;
    // if we hit this assert, it's likely that the data producer isn't sticking
    // headers after a token to delineate a new part. This is required. If
    // the server's not sending those headers, the server's broken.
	NS_ASSERTION(mPartChannel, "our channel went away :-(");
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

PRInt8 
nsMultiMixedConv::PushOverLine(char *&aPtr, PRUint32 &aLen) {
    PRInt8 chars = 0;
    if (*aPtr == CR || *aPtr == LF) {
        if (aPtr[1] == LF)
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
    PRUint8 lineFeedIncrement = 1;

    while ( (cursorLen > 0) 
            && (newLine = PL_strchr(cursor, LF)) ) {
        // adjust for linefeeds
        if ( (newLine > cursor) && (newLine[-1] == CR) ) { // CRLF
             lineFeedIncrement = 2;
             newLine--;
        }

        if (newLine == cursor) {
            // move the newLine beyond the double linefeed marker
            if ( (newLine - cursor) < ((PRInt32)cursorLen - lineFeedIncrement) ) {
                newLine += lineFeedIncrement;
            }
            cursorLen -= (newLine - cursor);
            cursor = newLine;

            done = PR_TRUE;
            mPartCount++;
            break;
        }

        char tmpChar = *newLine;
        *newLine = '\0';
        char *colon = PL_strchr(cursor, ':');
        if (colon) {
            *colon = '\0';
            nsCAutoString headerStr(cursor);
            headerStr.CompressWhitespace();
            headerStr.ToLowerCase();
            nsCOMPtr<nsIAtom> header = NS_NewAtom(headerStr.GetBuffer());
            if (!header) return NS_ERROR_OUT_OF_MEMORY;
            *colon = ':';

            nsCAutoString headerVal(colon + 1);
            headerVal.CompressWhitespace();

            // examine header
            if (headerStr.Equals("content-type")) {
                mContentType = headerVal;
            } else if (headerStr.Equals("content-length")) {
                mContentLength = atoi(headerVal.GetBuffer());
            } else if (headerStr.Equals("set-cookie")) {
                // setting headers on the HTTP channel
                // causes HTTP to notify, again if necessary,
                // it's header observers.
                nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(aChannel);
                if (httpChannel) {
                    rv = httpChannel->SetResponseHeader(header, headerVal);
                    if (NS_FAILED(rv)) return rv;
                }
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
                        mToken = newToken.GetBuffer();
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
    return (*aMultiMixedConv)->Init();
}

