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
#include "nsIAllocator.h"
#include "plstr.h"
#include "nsIStringStream.h"
#include "nsIHTTPChannel.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

// other converters for the converter export factory fuctions
#include "nsFTPDirListingConv.h"


// nsISupports implementation
NS_IMPL_ISUPPORTS3(nsMultiMixedConv, nsIStreamConverter, nsIStreamListener, nsIStreamObserver);


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

// nsIStreamListener implementation
NS_IMETHODIMP
nsMultiMixedConv::OnDataAvailable(nsIChannel *channel, nsISupports *context,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    nsresult rv;
    char *buffer = nsnull, *rootMemPtr = nsnull;
    PRUint32 bufLen, read;

    NS_ASSERTION(channel, "multimixed converter needs a channel");

    if (!mBoundaryCStr) {
        char *bndry = nsnull;
        nsXPIDLCString delimiter;

        // ask the HTTP channel for the content-type and extract the boundary from it.
        nsCOMPtr<nsIHTTPChannel> httpChannel;
        rv = channel->QueryInterface(NS_GET_IID(nsIHTTPChannel), getter_AddRefs(httpChannel));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIAtom> header = NS_NewAtom("content-type");
            if (!header) return NS_ERROR_OUT_OF_MEMORY;
            rv = httpChannel->GetResponseHeader(header, getter_Copies(delimiter));
            if (NS_FAILED(rv)) return rv;
        } else {
            // try asking the channel directly
            rv = channel->GetContentType(getter_Copies(delimiter));
            if (NS_FAILED(rv)) return rv;
        }
        if (!delimiter) return NS_ERROR_FAILURE;
        bndry = PL_strstr(delimiter, "boundary");
        if (!bndry) return NS_ERROR_FAILURE;

        bndry = PL_strchr(bndry, '=');
        if (!bndry) return NS_ERROR_FAILURE;

        bndry++; // move past the equals sign

        nsCAutoString boundaryString(bndry);
        boundaryString.StripWhitespace();
        // we're not using the beginning "--" to delimit boundaries
        // so just make them part of the boundary delimiter.
        boundaryString.Insert("--", 0, 2);                 ;
        mBoundaryCStr = boundaryString.ToNewCString();
        if (!mBoundaryCStr) return NS_ERROR_OUT_OF_MEMORY;
        mBoundaryStrLen = boundaryString.Length();
    }


    rv = inStr->Available(&bufLen);
    if (NS_FAILED(rv)) return rv;

    rootMemPtr = buffer = (char*)nsAllocator::Alloc(bufLen + 1);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    rv = inStr->Read(buffer, bufLen, &read);
    if (NS_FAILED(rv) || read == 0) return rv;
    buffer[bufLen] = '\0';

    if (mBufferedData.Length() > 0) {
        mBufferedData.Append(buffer);
        nsAllocator::Free(buffer);
        buffer = mBufferedData.ToNewCString();
        mBufferedData.Truncate(0);
    }
    char *cursor = buffer;
    PRBool done = PR_FALSE;

    while (cursor) {
        char *boundary = PL_strstr(cursor, mBoundaryCStr);
        if (boundary) {
            mFoundBoundary = PR_TRUE;
            if (cursor == boundary) {
                if (!mNewPart) {
                    // we were processing a part and ran into a boundary
                    // thus that makes it an ending boundary. finish this
                    // part and move on.
                    rv = mFinalListener->OnStopRequest(mPartChannel, context, NS_OK, nsnull);
                    if (NS_FAILED(rv)) break;

                    // Remove the channel from its load group (if any)
                    nsCOMPtr<nsILoadGroup> loadGroup;
            
                    (void) mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));
                    if (loadGroup) {
                        loadGroup->RemoveChannel(mPartChannel, context, NS_OK, nsnull);
                    }

                    mPartChannel = 0;
                }
                // the boundary occurs at the beginning of the data.
                // we obviously don't want to set it to null, instead
                // set the cursor to the beginnning of the real data
                // and look for another boundary after that.
                cursor += mBoundaryStrLen;
                if (*cursor == '-') {
                    // we've reached the end of the line
                    done = PR_TRUE;
                } else {
                    // we just passed a boundary, therefore we're guaranteed to be
                    // processing a new part.
                    mNewPart = PR_TRUE;
                    // see if there's another boundary.
                    boundary = PL_strstr(cursor, mBoundaryCStr);
                    if (boundary) *boundary = '\0';
                }

                if ( (*cursor == CR) || (*cursor == LF) ) {
                    if (cursor[1] == LF)
                        cursor++; // if it's a CRLF, move two places.
                    cursor++;
                }
            } else {
                *boundary = '\0';
            }
        } else if (!mFoundBoundary) {
            // we haven't seen a boundary yet, buffer this up.
            mBufferedData = cursor;
            break; // XXX do more here.
        }
        if (mNewPart) {
            mLineFeedIncrement = 1;
            // check for a newline char, then figure out if
            // we're dealing w/ CRLFs as line delimiters.
            // unfortunately we can see both :(
            char *newLine = PL_strchr(cursor, LF);
            if (newLine) {
                if ( (newLine > cursor) && (newLine[-1] == CR) ) {
                     // CRLF
                     mLineFeedIncrement = 2;
                     newLine--;
                }
            }
            char *headerStart = cursor;
            while (newLine) {
                *newLine = '\0';
                char *colon = PL_strchr(headerStart, ':');
                if (colon) {
                    // Header name
                    *colon = '\0';
                    nsCAutoString headerStr(headerStart);
                    headerStr.StripWhitespace();
                    headerStr.ToLowerCase();
                    nsCOMPtr<nsIAtom> header = NS_NewAtom(headerStr.GetBuffer());
                    if (!header) {
                        nsAllocator::Free(buffer);
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                    *colon = ':';
                    // END Header name

                    // Header value
                    nsCAutoString headerVal(colon + 1);
                    headerVal.StripWhitespace();
                    // END Header value

                    // examine header
                    if (headerStr.Equals("content-type")) {
                        mContentType = headerVal;
                    } else if (headerStr.Equals("content-length")) {
                        mContentLength = atoi(headerVal);
                    } else if (headerStr.Equals("set-cookie")) {
                        // setting headers on the HTTP channel
                        // causes HTTP to notify, again if necessary,
                        // it's header observers.
                        nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
                        if (httpChannel) {
                            rv = httpChannel->SetResponseHeader(header, headerVal);
                            if (NS_FAILED(rv)) {
                                nsAllocator::Free(buffer);
                                return rv;
                            }
                        }
                    }
                }
                if (    (newLine[mLineFeedIncrement] == LF)
                     || (newLine[mLineFeedIncrement] == CR)
                     || (newLine == cursor) ) {
                    nsCOMPtr<nsILoadGroup> loadGroup;

                    // that's it we've processed all the headers and 
                    // this is no longer a mNewPart
                    mNewPart = PR_FALSE;

                    // move the newLine beyond the double linefeed marker
                    newLine += mLineFeedIncrement;

                    // First build up a dummy uri.
                    nsCOMPtr<nsIURI> partURI;
                    rv = BuildURI(channel, getter_AddRefs(partURI));
                    if (NS_FAILED(rv)) {
                        nsAllocator::Free(buffer);
                        return rv;
                    }
                    (void) channel->GetLoadGroup(getter_AddRefs(loadGroup));

                    if (mContentType.Length() < 1)
                        mContentType = "text/html"; // default to text/html, that's all we'll ever see anyway
                    rv = NS_NewInputStreamChannel(getter_AddRefs(mPartChannel), 
                                                  partURI, 
                                                  nsnull,       // inStr
                                                  mContentType.GetBuffer(), 
                                                  mContentLength);
                    if (NS_FAILED(rv)) goto error;

                    // Add the new channel to the load group (if any)
                    if (loadGroup) {
                        rv = mPartChannel->SetLoadGroup(loadGroup);
                        if (NS_FAILED(rv)) goto error;
                        rv = loadGroup->AddChannel(mPartChannel, nsnull);
                        if (NS_FAILED(rv)) goto error;
                    }

                    // Let's start off the load. NOTE: we don't forward on the channel passed
                    // into our OnDataAvailable() as it's the root channel for the raw stream.
                    rv = mFinalListener->OnStartRequest(mPartChannel, context);
                  error:
                    if (NS_FAILED(rv)) {
                        nsAllocator::Free(buffer);
                        return rv;
                    }
                    break;
                }
                headerStart = newLine + mLineFeedIncrement;
                newLine = PL_strchr(headerStart, LF);
                if (newLine) {
                    // we're catching LF, LFLF, and CRLF
                    if ( (newLine > cursor) && (newLine[-1] == CR) ) {
                         mLineFeedIncrement = 2;
                         newLine--;
                    }
                }
            } // end while (newLine)

            if (mNewPart) {
                // we broke out because we have incomplete headers
                // buffer the data (if there is any) and fall out.
                if (*headerStart)
                    mBufferedData = headerStart;
                break;
            } else {
                mPartCount++;
                // we broke out because we reached two linefeeds (noting
                // the end of the header section. move the cursor over the
                // linefeeds into the data and start processing it.
                if (cursor == newLine) {
                    // all we got was a newline in this read. kick out
                    break;
                } else {
                    cursor = newLine + mLineFeedIncrement;
                }
            }
        } // end mNewPart

        // after processing headers, it's possible that cursor points to
        // a null byte. If it does don't send it off because it's meaningless.
        if (!*cursor)
            break;

        // Check for a completed part
        if (!done) {
            // If we've gotten this far we know we're processing a hunk of data
            // and that all the headers for this part have been processed.
            // Just send off the data.
            SendData(cursor, mPartChannel, context);
        }

        if (boundary) {
            // We know this is the end of a part. Set mNewPart to TRUE
            // so anymore data we receive gets kicked into the mNewPart
            // cycle, tell the listener we're done w/ this part, reset
            // the partChannel (mNewPart will create a new one if we
            // get that far), and see if we're completely done.
            mNewPart = PR_TRUE;
            rv = mFinalListener->OnStopRequest(mPartChannel, context, NS_OK, nsnull);
            if (NS_FAILED(rv)) break;

            // Remove the channel from its load group (if any)
            nsCOMPtr<nsILoadGroup> loadGroup;
            
            (void) mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));
            if (loadGroup) {
                loadGroup->RemoveChannel(mPartChannel, context, NS_OK, nsnull);
            }

            mPartChannel = 0; // kill this channel. it's done
            if (done || (*(boundary+mBoundaryStrLen+1) == '-') ) {
                // it's completely over
                break;
            } else {
                cursor = boundary + mBoundaryStrLen;
            }
            if ( (*cursor == CR) || (*cursor == LF) ){
                if (cursor[1] == LF)
                    cursor++; // if it's a CRLF move two places.
                cursor++;
            }
        } else {
            // no more data. kick out
            break; 
        }
    } // end while (cursor)
    nsAllocator::Free(buffer);

    return rv;
}


// nsIStreamObserver implementation
NS_IMETHODIMP
nsMultiMixedConv::OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
    return NS_OK;
}

NS_IMETHODIMP
nsMultiMixedConv::OnStopRequest(nsIChannel *channel, nsISupports *ctxt,
                                nsresult status, const PRUnichar *errorMsg) {
    return NS_OK;
}


// nsMultiMixedConv methods
nsMultiMixedConv::nsMultiMixedConv() {
    NS_INIT_ISUPPORTS();
    mBoundaryCStr       = nsnull;
    mBoundaryStrLen     = mPartCount = 0;
    mNewPart            = PR_TRUE;
    mFoundBoundary      = PR_FALSE;
    mContentLength      = -1;
    mLineFeedIncrement  = 1;
}

nsMultiMixedConv::~nsMultiMixedConv() {
    if (mBoundaryCStr) nsAllocator::Free(mBoundaryCStr);
}

nsresult
nsMultiMixedConv::Init() {
    return NS_OK;
}


nsresult
nsMultiMixedConv::SendData(const char *aBuffer, nsIChannel *aChannel, nsISupports *aCtxt) {

    nsresult rv;
    nsCOMPtr<nsISupports> inStreamSup;
    rv = NS_NewStringInputStream(getter_AddRefs(inStreamSup), aBuffer);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStream> inStream = do_QueryInterface(inStreamSup, &rv);
    if (NS_FAILED(rv)) return rv;

    PRUint32 len;
    rv = inStream->Available(&len);
    if (NS_FAILED(rv)) return rv;

    return mFinalListener->OnDataAvailable(aChannel, aCtxt, inStream, 0, len);
}

nsresult
nsMultiMixedConv::BuildURI(nsIChannel *aChannel, nsIURI **_retval) {
    nsresult rv;
    nsXPIDLCString uriSpec;
    nsCOMPtr<nsIURI> rootURI;
    rv = aChannel->GetURI(getter_AddRefs(rootURI));
    if (NS_FAILED(rv)) return rv;

    rv = rootURI->GetSpec(getter_Copies(uriSpec));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString dummyURIStr(uriSpec);
    dummyURIStr.Append("##");
    dummyURIStr.Append((PRInt32) mPartCount, 10 /* radix */);

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    return serv->NewURI(dummyURIStr.GetBuffer(), nsnull, _retval);
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
