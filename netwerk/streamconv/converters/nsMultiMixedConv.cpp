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

#include "nsMultiMixedConv.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "nsIStringStream.h"
#include "nsIHTTPChannel.h"
#include "nsIAtom.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsIIOService.h"
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);


// nsISupports implementation
NS_IMPL_ISUPPORTS2(nsMultiMixedConv, nsIStreamConverter, nsIStreamListener);


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
    NS_ADDREF(mFinalListener);

    return NS_OK;
}


// nsIStreamListener implementation
NS_IMETHODIMP
nsMultiMixedConv::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt,
                                  nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    nsresult rv;
    char *buffer = nsnull, *rootMemPtr = nsnull;
    PRUint32 bufLen, read;

    NS_ASSERTION(channel, "multimixed converter needs a channel");

    if (!mBoundaryCStr) {
        // ask the channel for the content-type and extract the boundary from it.
        nsIHTTPChannel *httpChannel = nsnull;
        rv = channel->QueryInterface(NS_GET_IID(nsIHTTPChannel), (void**)&httpChannel);
        if (NS_SUCCEEDED(rv)) {
            char *bndry = nsnull, *delimiter = nsnull;
            nsIAtom *header = NS_NewAtom("content-type");
            if (!header) return NS_ERROR_OUT_OF_MEMORY;
            rv = httpChannel->GetResponseHeader(header, &delimiter);
            NS_RELEASE(header);
            NS_RELEASE(httpChannel);
            if (NS_FAILED(rv)) return rv;

            bndry = PL_strstr(delimiter, "boundary");
            if (!bndry) return NS_ERROR_FAILURE;

            bndry = PL_strchr(bndry, '=');
            if (!bndry) return NS_ERROR_FAILURE;

            bndry++; // move past the equals sign

            nsCString boundaryString(bndry);
            boundaryString.StripWhitespace();
            mBoundaryCStr = boundaryString.ToNewCString();
            if (!mBoundaryCStr) return NS_ERROR_OUT_OF_MEMORY;
            mBoundaryStrLen = boundaryString.Length();
        } else {
            // we couldn't get at the boundary and we need one.
            NS_ASSERTION(0, "no http channel to get the multipart boundary from");
            return NS_ERROR_FAILURE;
        }
    }

    rv = inStr->GetLength(&bufLen);
    if (NS_FAILED(rv)) return rv;

    rootMemPtr = buffer = (char*)nsAllocator::Alloc(bufLen + 1);
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    rv = inStr->Read(buffer, bufLen, &read);
    if (NS_FAILED(rv)) return rv;
    buffer[bufLen] = '\0';

    // Break up the stream into it sub parts and send off an On*() triple combination
    // for each subpart.

    // This design of HTTP's multipart/x-mixed-replace (see nsMultiMixedConv.h for more info)
    // was flawed from the start. This parsing makes the following, poor, assumption about
    // the data coming from the server:
    // 
    //  - The server will never send a partial boundary token
    // 
    // This assumption is necessary in order for this type to be handled. Also note that
    // the server doesn't send any Content-Length information.
    char *boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
    do {
        if (boundaryLoc) {
            // check for another
            char *boundaryLoc2 = PL_strstr(boundaryLoc + mBoundaryStrLen, mBoundaryCStr);

            if (!mBoundaryStart) {
                // see if this is the end
                if (*(boundaryLoc + mBoundaryStrLen + 1) == '-')
                    break;

                mPartCount++;
                // This is the start of a part. We want to build up a new stub
                // channel and call through to our listener.
                mBoundaryStart = PR_TRUE;
                nsCString contentTypeStr;

                NS_IF_RELEASE(mPartChannel);

                // First build up a dummy uri.
                nsIURI *partURI = nsnull;
                rv = BuildURI(channel, &partURI);
                if (NS_FAILED(rv)) return rv;

                // check for any headers in this part
                char *headersEnd = PL_strstr(boundaryLoc + mBoundaryStrLen, "\n\n");
                if (headersEnd) {
                    char *headerStart = boundaryLoc + mBoundaryStrLen + 1;
                    char *headerCStr = nsnull;
                    while ( (headerCStr = PL_strchr(headerStart, '\n')) ) {
                        *headerCStr = '\0';

                        char *colon = PL_strchr(headerStart, ':');
                        if (colon) {
                            *colon = '\0';
                            nsCString headerStr(headerStart);
                            headerStr.ToLowerCase();
                            nsIAtom *header = NS_NewAtom(headerStr.GetBuffer());
                            *colon = ':';

                            nsCString headerVal(colon + 1);
                            headerVal.StripWhitespace();

                            if (headerStr == "content-type") {
                                contentTypeStr = headerVal;
                            } else {
                                // XXX we need a way to set other header's such as cookies :/
                                // XXX maybe we just handle cookies directly here.
                            }
                        }
                        
                        *headerCStr = '\n';
                        if (headerCStr[1] == '\n') break;
                        // increment and move on.
                        headerStart = headerCStr + 1;
                    }
                }

                NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
                if (NS_FAILED(rv)) return rv;

                if (contentTypeStr.Length() < 1)
                    contentTypeStr = "text/html"; // default to text/html, that's all we'll ever see anyway
                rv = serv->NewInputStreamChannel(partURI, contentTypeStr.GetBuffer(),
                                                 nsnull, &mPartChannel);
                NS_RELEASE(partURI);
                if (NS_FAILED(rv)) return rv;

                // Let's start off the load. NOTE: we don't forward on the channel passed
                // into our OnDataAvailable() as it's the root channel for the raw stream.
                rv = mFinalListener->OnStartRequest(mPartChannel, ctxt);
                if (NS_FAILED(rv)) return rv;


                // pull the boundary loc, and headers from the stream.
                if (headersEnd)
                    // even though we're at the end of the headers, there might not be any data
                    // so be sure to check for null byte before sending it off. the parser doesn't
                    // like parsing zero length streams. :).
                    boundaryLoc = headersEnd + 2;
                else
                    boundaryLoc += mBoundaryStrLen;

                if (boundaryLoc2) {
                    // there was another boundary loc (signifying the end of this part)
                    // in the buffer. Build up this hunk and fire it off.
                    mBoundaryStart = PR_FALSE;
                    char tmpChar = *boundaryLoc2;
                    *boundaryLoc2 = '\0';

                    rv = SendData(boundaryLoc, mPartChannel, ctxt);
                    if (NS_FAILED(rv)) return rv;

                    *boundaryLoc2 = tmpChar;

                    rv = mFinalListener->OnStopRequest(mPartChannel, ctxt, NS_OK, nsnull);
                    if (NS_FAILED(rv)) return rv;

                    // increment the buffer and start over.
                    buffer = boundaryLoc2;
                    
                    boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
                } else {
                    // fire off what we've got
                    buffer = boundaryLoc;

                    // null byte check
                    if (*buffer) { 
                        rv = SendData(buffer, mPartChannel, ctxt);
                        if (NS_FAILED(rv)) return rv;
                    }
                    boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
                }
            } else {
                // this boundaryLoc is an ending delimiter.
                mBoundaryStart = PR_FALSE;
                char tmpChar = *boundaryLoc;
                *boundaryLoc = '\0';

                if (*buffer) {
                    rv = SendData(buffer, mPartChannel, ctxt);
                    if (NS_FAILED(rv)) return rv;
                }

                *boundaryLoc = tmpChar;

                rv = mFinalListener->OnStopRequest(mPartChannel, ctxt, NS_OK, nsnull);
                if (NS_FAILED(rv)) return rv;

                buffer = boundaryLoc;
                boundaryLoc = PL_strstr(buffer, mBoundaryCStr);
            }
        } else {
            // null byte check
            if (*buffer) { 
                rv = SendData(buffer, mPartChannel, ctxt);
                if (NS_FAILED(rv)) return rv;
            }
        }
    } while (boundaryLoc);

    nsAllocator::Free(rootMemPtr);

    return NS_OK;
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
    mFinalListener      = nsnull;
    mBoundaryCStr       = nsnull;
    mPartChannel        = nsnull;
    mBoundaryStrLen     = mPartCount = 0;
}

nsMultiMixedConv::~nsMultiMixedConv() {
    NS_IF_RELEASE(mFinalListener);
    if (mBoundaryCStr) nsAllocator::Free(mBoundaryCStr);
    NS_IF_RELEASE(mPartChannel);
}

nsresult
nsMultiMixedConv::Init() {
    mBoundaryStart = PR_FALSE;
    return NS_OK;
}


nsresult
nsMultiMixedConv::SendData(const char *aBuffer, nsIChannel *aChannel, nsISupports *aCtxt) {

    nsresult rv;
    nsISupports *stringStreamSup = nsnull;
    rv = NS_NewStringInputStream(&stringStreamSup, aBuffer);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream *inStr = nsnull;
    rv = stringStreamSup->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inStr);
    NS_RELEASE(stringStreamSup);
    if (NS_FAILED(rv)) return rv;

    PRUint32 len;
    rv = inStr->GetLength(&len);
    if (NS_FAILED(rv)) return rv;

    rv = mFinalListener->OnDataAvailable(aChannel, aCtxt, inStr, 0, len);
    NS_RELEASE(inStr);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

nsresult
nsMultiMixedConv::BuildURI(nsIChannel *aChannel, nsIURI **_retval) {
    nsresult rv;
    char *uriSpec = nsnull;
    nsIURI *rootURI = nsnull;
    rv = aChannel->GetURI(&rootURI);
    if (NS_FAILED(rv)) return rv;

    rv = rootURI->Clone(_retval);
    NS_RELEASE(rootURI);
    if (NS_FAILED(rv)) return rv;

    rv = (*_retval)->GetSpec(&uriSpec);
    if (NS_FAILED(rv)) return rv;

    nsCString dummyURIStr(uriSpec);
    dummyURIStr.Append("##");
    dummyURIStr.Append(mPartCount);

    char *dummyCStr = dummyURIStr.ToNewCString();
    rv = (*_retval)->SetSpec(dummyCStr);
    nsAllocator::Free(dummyCStr);
    return rv;    
}



////////////////////////////////////////////////////////////////////////
// Factory
////////////////////////////////////////////////////////////////////////
MultiMixedFactory::MultiMixedFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_ISUPPORTS();
}

MultiMixedFactory::~MultiMixedFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMPL_ISUPPORTS(MultiMixedFactory, NS_GET_IID(nsIFactory));

NS_IMETHODIMP
MultiMixedFactory::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv = NS_OK;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kMultiMixedConverterCID)) {
        nsMultiMixedConv *conv = new nsMultiMixedConv();
        if (!conv) return NS_ERROR_OUT_OF_MEMORY;
        rv = conv->Init();
        if (NS_FAILED(rv)) return rv;

        rv = conv->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(inst);
    *aResult = inst;
    NS_RELEASE(inst);
    return rv;
}

nsresult
MultiMixedFactory::LockFactory(PRBool aLock){
    return NS_OK;
}

// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory) {
    nsresult rv;
    if (aFactory == nsnull)
        return NS_ERROR_NULL_POINTER;

    nsIGenericFactory* fact;
    if (aClass.Equals(kMultiMixedConverterCID)) {
        rv = NS_NewGenericFactory(&fact, nsMultiMixedConv::Create);
    }
    else {
        rv = NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(rv))
        *aFactory = fact;
    return rv;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath) {
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kMultiMixedConverterCID,
                                    "MultiMixedConverter",
                                    "component:||netscape|streamConverters|multimixedconverter",
                                    aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath) {
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kMultiMixedConverterCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// Factory END
////////////////////////////////////////////////////////////////////////

