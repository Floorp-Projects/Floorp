/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Author: Gagan Saksena <gagan@netscape.com>
 *
 * Contributor(s): 
 *   Adrian Havill <havill@redhat.com>
 */

#include "nspr.h"
#include "nsIURL.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPRequest.h"
#include "nsHTTPAtoms.h"
#include "nsHTTPEnums.h"
#include "nsIPipe.h"
#include "nsIStringStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponseListener.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"
#include "nsIIOService.h"
#include "nsAuthEngine.h"
#include "nsIServiceManager.h"
#include "nsISocketTransport.h"
#include "nsISSLSocketControl.h"
#include "plstr.h"
#include "nsNetUtil.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif

#define LOG(args) PR_LOG(gHTTPLog, PR_LOG_DEBUG, args)

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID, NS_IHTTPHANDLER_CID);

extern nsresult DupString(char **aDest, const char *aSrc);

nsHTTPRequest::nsHTTPRequest(nsIURI *aURL, 
                             nsHTTPHandler *aHandler)
    : mPipelinedRequest(nsnull)
    , mDoingProxySSLConnect(PR_FALSE)
    , mSSLConnected(PR_FALSE)
    , mVersion(HTTP_ONE_ZERO)
    , mKeepAliveTimeout(0)
    , mRequestSpec(0)
    , mHandler(aHandler)
    , mAbortStatus(NS_OK)
    , mHeadersFormed(PR_FALSE)
    , mPort(-1)
    , mProxySSLConnectAllowed(PR_FALSE)
{   
    NS_INIT_ISUPPORTS();

    NS_ASSERTION(aURL, "No URL for the request!!");
    mURI = do_QueryInterface(aURL);

    mMethod = nsHTTPAtoms::Get;

#if defined(PR_LOGGING)
    nsXPIDLCString urlCString; 
    mURI->GetSpec(getter_Copies(urlCString));
  
    LOG(("Creating nsHTTPRequest [this=%p] for URI: %s.\n", 
        this, (const char *)urlCString));
#endif
  
    mHandler->GetHttpVersion(&mVersion);
    mHandler->GetKeepAliveTimeout(&mKeepAliveTimeout);
    mHandler->GetProxySSLConnectAllowed(&mProxySSLConnectAllowed);

    mURI->GetSpec(getter_Copies(mSpec));
    mURI->GetHost(getter_Copies(mHost));
    mURI->GetPort(&mPort);
}
    
nsHTTPRequest::~nsHTTPRequest()
{
    LOG(("Deleting nsHTTPRequest [this=%x].\n", this));
    CRTFREEIF(mRequestSpec);
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

// XXX why does this need to be threadsafe??
NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTTPRequest, nsIRequest)

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsHTTPRequest::GetName(PRUnichar **result)
{
    if (mPipelinedRequest)
        return mPipelinedRequest->GetName(result);

    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mURI->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;

    nsString name;
    name.AppendWithConversion(urlStr);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsHTTPRequest::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;

    if (mPipelinedRequest)
        rv = mPipelinedRequest->IsPending(result);
    else
        *result = PR_FALSE; 

    return rv;
}

NS_IMETHODIMP
nsHTTPRequest::GetStatus(nsresult *status)
{
    *status = mAbortStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::Cancel(nsresult status)
{
    nsresult rv = NS_OK;

    mAbortStatus = status;
    if (mPipelinedRequest)
        rv = mPipelinedRequest->Cancel(status);

    return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Suspend()
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mPipelinedRequest)
        rv = mPipelinedRequest->Suspend();
    return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Resume()
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mPipelinedRequest)
        rv = mPipelinedRequest->Resume();
    return rv;
}

nsresult
nsHTTPRequest::Clone(const nsHTTPRequest **aRequest) const
{
    return NS_ERROR_FAILURE;
}
                        
nsresult
nsHTTPRequest::SetMethod(nsIAtom *aMethod)
{
    mMethod = aMethod;
    return NS_OK;
}
                        
nsresult
nsHTTPRequest::SetPriority()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsHTTPRequest::GetPriority()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsHTTPRequest::SetHeader(nsIAtom *aHeader, const char *aValue)
{
    return mHeaders.SetHeader(aHeader, aValue);
}

nsresult
nsHTTPRequest::GetHeader(nsIAtom *aHeader, char **aValue)
{
    return mHeaders.GetHeader(aHeader, aValue);
}

nsresult
nsHTTPRequest::SetHTTPVersion(PRUint32 aVersion)
{
    mVersion = aVersion;
    return NS_OK;
}

nsresult
nsHTTPRequest::GetHTTPVersion(PRUint32 *aVersion) 
{
    NS_ENSURE_ARG_POINTER(aVersion);
    *aVersion = mVersion;
    return NS_OK;
}

nsresult
nsHTTPRequest::SetConnection(nsHTTPChannel *aConnection)
{
    mConnection = aConnection;
    return NS_OK;
}

nsresult
nsHTTPRequest::GetConnection(nsHTTPChannel **aConnection)
{
    NS_ENSURE_ARG_POINTER(aConnection);
    *aConnection = mConnection;
    return NS_OK;
}

nsresult
nsHTTPRequest::SetTransport(nsITransport *aTransport)
{
    return NS_OK;
}

nsresult
nsHTTPRequest::GetTransport(nsITransport **aTransport)
{
    if (mPipelinedRequest)
        return mPipelinedRequest->GetTransport(aTransport);
    else
        *aTransport = nsnull;

    return NS_OK;
}

nsresult
nsHTTPRequest::GetHeaderEnumerator(nsISimpleEnumerator **aResult)
{
    return mHeaders.GetEnumerator(aResult);
}

nsresult
nsHTTPRequest::SetOverrideRequestSpec(const char *aSpec)
{
    CRTFREEIF(mRequestSpec);
    
    if (aSpec) {
        // proxy case
        if (mProxySSLConnectAllowed && !PL_strncasecmp(mSpec, "https", 5))
            mDoingProxySSLConnect = PR_TRUE;
    }
    
    return DupString(&mRequestSpec, aSpec);
}

nsresult
nsHTTPRequest::GetOverrideRequestSpec(char **aSpec)
{
    return DupString(aSpec, mRequestSpec);
}

nsresult
nsHTTPRequest::formHeaders(PRUint32 capabilities)
{
    if (mHeadersFormed)
        return NS_OK;

    // Send Host header by default
    if (HTTP_ZERO_NINE != mVersion) {
        if (-1 != mPort) {
            char *tempHostPort = 
                PR_smprintf(PL_strchr(mHost, ':') ? "[%s]:%d" : "%s:%d",
                            (const char*)mHost, mPort);
            if (tempHostPort) {
                SetHeader(nsHTTPAtoms::Host, tempHostPort);
                PR_smprintf_free(tempHostPort);
                tempHostPort = 0;
            }
        }
        else
            SetHeader(nsHTTPAtoms::Host, mHost);
    }

    nsresult rv;

    // Add the user-agent 
    nsXPIDLString ua;
    if (NS_SUCCEEDED(mHandler->GetUserAgent(getter_Copies(ua)))) {
        nsCAutoString uaString;
        uaString.AssignWithConversion(NS_STATIC_CAST(const PRUnichar*, ua));
        SetHeader(nsHTTPAtoms::User_Agent, uaString.get());
    }

    PRUint32 loadAttributes;
    mConnection->GetLoadAttributes(&loadAttributes);
    
    if (loadAttributes & 
            (nsIChannel::FORCE_VALIDATION | nsIChannel::FORCE_RELOAD)) {
        // We need to send 'Pragma:no-cache' to inhibit proxy caching even if
        // no proxy is configured since we might be talking with a transparent
        // proxy, i.e. one that operates at the network level.  See bug #14772
        SetHeader(nsHTTPAtoms::Pragma, "no-cache");
    }

    if (loadAttributes & nsIChannel::FORCE_RELOAD) {
        // If doing a reload, force end-to-end 
        SetHeader(nsHTTPAtoms::Cache_Control, "no-cache");
    }
    else if (loadAttributes & nsIChannel::FORCE_VALIDATION) {
        // A "max-age=0" cache-control directive forces each cache along the
        // path to the origin server to revalidate its own entry, if any, with
        // the next cache or server.
        SetHeader(nsHTTPAtoms::Cache_Control, "max-age=0");
    }

    // Send */*. We're no longer chopping MIME-types for acceptance.
    // MIME based content negotiation has died.
    // SetHeader(nsHTTPAtoms::Accept, "image/gif, image/x-xbitmap, image/jpeg, 
    // image/pjpeg, image/png, */*");
    SetHeader(nsHTTPAtoms::Accept, "*/*");

    nsXPIDLCString str;

    // Add the Accept-Language header
    rv = mHandler->GetAcceptLanguages(getter_Copies(str));
    if (NS_SUCCEEDED(rv) && str && *str)
        SetHeader(nsHTTPAtoms::Accept_Language, str);

    // Add the Accept-Encodings header
    rv = mHandler->GetAcceptEncodings(getter_Copies(str));
    if (NS_SUCCEEDED(rv) && str && *str)
        SetHeader(nsHTTPAtoms::Accept_Encoding, str);

    // Add the Accept-Charset header
    rv = mHandler->GetAcceptCharset(getter_Copies(str));
    if (NS_SUCCEEDED(rv) && str && *str)
        SetHeader(nsHTTPAtoms::Accept_Charset, str);

    if (capabilities &
            (nsIHTTPProtocolHandler::ALLOW_KEEPALIVE |
             nsIHTTPProtocolHandler::ALLOW_PROXY_KEEPALIVE)) {
        // Send keep-alive and connection request headers
        char *p = PR_smprintf("%d", mKeepAliveTimeout);
        
        SetHeader(nsHTTPAtoms::Keep_Alive, p);
        PR_smprintf_free(p);

        SetHeader(nsHTTPAtoms::Connection, "keep-alive");
    }
    else
        SetHeader(nsHTTPAtoms::Connection, "close");

    mHeadersFormed = PR_TRUE;
    return NS_OK;
}


nsresult
nsHTTPRequest::formBuffer(nsCString * requestBuffer, PRUint32 capabilities)
{
    nsXPIDLCString autoBuffer;
    nsresult rv;

    nsString methodString;
    nsCString cp;
    
    if (mDoingProxySSLConnect) { 
        nsCOMPtr<nsITransport> trans;
        PRUint32 reuse = 0;
        GetTransport(getter_AddRefs(trans));
        nsCOMPtr<nsISocketTransport> sockTrans = 
            do_QueryInterface(trans, &rv);
        if (NS_SUCCEEDED(rv))
            sockTrans->GetReuseCount(&reuse);
        if (reuse > 0) {
            mSSLConnected = PR_TRUE;
            mDoingProxySSLConnect = PR_FALSE;
        }
    }

    if (mDoingProxySSLConnect && !mSSLConnected) {
        requestBuffer->Append("CONNECT ");
        requestBuffer->Append(mHost);
        requestBuffer->Append(":");

        char tmp[20];
        sprintf(tmp, "%u",(mPort == -1) ? 
            ((!PL_strncasecmp(mSpec, "https", 5)) ? 443 : 80) 
                    : mPort);
        requestBuffer->Append(tmp);
        mSSLConnected = PR_TRUE;
    }
    else {
        mMethod->ToString(methodString);
        cp.AssignWithConversion(methodString);

        requestBuffer->Append(cp);
        requestBuffer->Append(" ");

        // Request spec gets set for proxied cases-
        // for proxied SSL form request just like non-proxy case
        if (!mRequestSpec || mSSLConnected) {
            rv = mURI->GetPath(getter_Copies(autoBuffer));
            requestBuffer->Append(autoBuffer);
        }
        else
            requestBuffer->Append(mRequestSpec);
    
        //Trim off the # portion if any...
        int refLocation = requestBuffer->RFind("#");
    
        if (-1 != refLocation)
            requestBuffer->Truncate(refLocation);
    }

    char *httpVersion = " HTTP/1.0" CRLF;

    switch (mVersion) {
        case HTTP_ZERO_NINE:
            httpVersion = " HTTP/0.9" CRLF;
            break;
        case HTTP_ONE_ONE:
            if (!(capabilities & NS_STATIC_CAST(unsigned long, nsIHTTPProtocolHandler::DONTALLOW_HTTP11)))
                httpVersion = " HTTP/1.1" CRLF;
    }

    requestBuffer->Append(httpVersion);

    //
    // Write the request headers, if any...
    //
    // ie. field-name ":" [field-value] CRLF
    //
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = mHeaders.GetEnumerator(getter_AddRefs(enumerator));

    if (NS_SUCCEEDED(rv)) {
        PRBool bMoreHeaders;
        nsCOMPtr<nsISupports>   item;
        nsCOMPtr<nsIHTTPHeader> header;
        nsCOMPtr<nsIAtom>       headerAtom;

        enumerator->HasMoreElements(&bMoreHeaders);
        while (bMoreHeaders) {
            enumerator->GetNext(getter_AddRefs(item));
            header = do_QueryInterface(item);

            NS_ASSERTION(header, "Bad HTTP header.");
            
            if (header) {
                header->GetField(getter_AddRefs(headerAtom));
                nsXPIDLCString fieldName;
                header->GetFieldName(getter_Copies(fieldName));
                NS_ASSERTION(fieldName, "field name not returned!, \
                        out of memory?");
                requestBuffer->Append(fieldName);
                header->GetValue(getter_Copies(autoBuffer));

                requestBuffer->Append(": ");
                requestBuffer->Append(autoBuffer);
                requestBuffer->Append(CRLF);
            }
            enumerator->HasMoreElements(&bMoreHeaders);
        }
    }

    // This is under the assumpsion that the POST/PUT headers are all done
    if (!mInputStream  || mDoingProxySSLConnect)
        requestBuffer->Append(CRLF);

    return NS_OK;
}

static PRUint32 sPipelinedRequestCreated = 0;
static PRUint32 sPipelinedRequestDeleted = 0;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTTPPipelinedRequest, nsIStreamObserver)

nsHTTPPipelinedRequest::nsHTTPPipelinedRequest(nsHTTPHandler *aHandler,
                                               const char *host,
                                               PRInt32 port,
                                               PRUint32 capabilities)
    : mCapabilities(capabilities)
    , mAttempts(0)
    , mMustCommit(PR_FALSE)
    , mTotalProcessed(0)
    , mTotalWritten(0)
    , mHandler(aHandler)
    , mPort(port)
    , mListener(nsnull)
    , mOnStopDone(PR_TRUE)
{   
    NS_INIT_ISUPPORTS();

    mHost = host;
    LOG(("Creating nsHTTPPipelinedRequest [this=%p], created=%d, deleted=%d\n",
        this, ++sPipelinedRequestCreated, sPipelinedRequestDeleted));

    NS_NewISupportsArray(getter_AddRefs(mRequests));

}

nsHTTPPipelinedRequest::~nsHTTPPipelinedRequest()
{
    PRUint32 count = 0;
    PRInt32  index;

    LOG(("Deleting nsHTTPPipelinedRequest [this=%p], created=%d, deleted=%d\n",
        this, sPipelinedRequestCreated, ++sPipelinedRequestDeleted));

    if (mRequests) {
        mRequests->Count(&count);
        if (count > 0) {
            for (index =(PRInt32) count - 1; index >= 0; --index) {
                nsHTTPRequest * req =(nsHTTPRequest *) mRequests->ElementAt(index);
                if (req != NULL) {
                    req->mPipelinedRequest = nsnull;
                    mRequests->RemoveElement(req);
                    NS_RELEASE(req);
                }
            }
        }
    }
}

nsresult
nsHTTPPipelinedRequest::SetTransport(nsITransport *aTransport)
{
    mTransport = aTransport;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetTransport(nsITransport **aTransport)
{
    NS_ENSURE_ARG_POINTER(aTransport);
    NS_IF_ADDREF(*aTransport = mTransport);
    return NS_OK;
}

// Finally our own methods...

nsresult
nsHTTPPipelinedRequest::WriteRequest(nsIInputStream *aRequestStream)
{
    nsresult rv = NS_OK;

    PRUint32 count = 0;
    PRUint32 index;

    LOG(("nsHTTPPipelinedRequest::WriteRequest()[%p], mOnStopDone=%d, mTransport=%x\n",
        this, mOnStopDone, mTransport.get()));

    if (!mRequests)
        return NS_ERROR_FAILURE;

    if (!mOnStopDone)
        return NS_OK;

    mRequests->Count(&count);

    if (count == 0 || mTotalWritten - mTotalProcessed >= count)
        return NS_OK;

    if (mAttempts > 1)
        return NS_ERROR_FAILURE;

    nsHTTPRequest *req = (nsHTTPRequest *) mRequests->ElementAt(0);

    req->GetUploadStream(getter_AddRefs(mInputStream));

    if (aRequestStream && !mInputStream)
        mInputStream = aRequestStream;

    if (!mTransport) {
        PRUint32 tMode = mAttempts ? TRANSPORT_OPEN_ALWAYS : TRANSPORT_REUSE_ALIVE;
        if (mInputStream)
            tMode &= ~(TRANSPORT_REUSE_ALIVE);

        rv = mHandler->RequestTransport(req->mURI, req->mConnection,  
                                           getter_AddRefs(mTransport), tMode);

        if (NS_FAILED(rv)) {
            NS_RELEASE(req);
            return rv;
        }

#if defined(PR_LOGGING)
        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(mTransport, &rv);
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString host;
        PRInt32 port;
        sTrans->GetHost(getter_Copies(host));
        sTrans->GetPort(&port);

        LOG(("nsHTTPPipelinedRequest: Got a socket transport [trans=%x, host=%s, port=%u]\n",
            mTransport.get(), (const char *) host, port));
#endif
    }
    
    NS_RELEASE(req);

    for (index = mTotalWritten - mTotalProcessed; index < count; index++) {
        req =(nsHTTPRequest *) mRequests->ElementAt(index);
        req->formHeaders(mCapabilities);
        NS_RELEASE(req);
    }

    mRequestBuffer.Truncate();

    for (index = mTotalWritten - mTotalProcessed; index < count; index++) {
        req =(nsHTTPRequest *) mRequests->ElementAt(index);
        req->formBuffer(&mRequestBuffer, mCapabilities);
        NS_RELEASE(req);
        mTotalWritten++;
    }

    //
    // Build up the request into mRequestBuffer...
    //
    LOG(("\nnsHTTPRequest::Build() [this=%p].\tWriting Request:\n"
         "=== Start\n%s=== End\n",
        this, mRequestBuffer.get()));

    //
    // Create a string stream to feed to the socket transport.  This stream
    // contains only the request.  Any PUT/POST data will be sent in a second
    // AsyncWrite to the server..
    //
    nsCOMPtr<nsISupports>    result;
    nsCOMPtr<nsIInputStream> stream;

    rv = NS_NewCharInputStream(getter_AddRefs(result), mRequestBuffer.get());
    if (NS_FAILED(rv)) return rv;

    stream = do_QueryInterface(result, &rv);
    if (NS_FAILED(rv)) return rv;

    req =(nsHTTPRequest *) mRequests->ElementAt(0);
    if (!req)
        return NS_ERROR_NULL_POINTER;

    //
    // Propagate the load attributes from the HTTPChannel into the
    // transport...
    //
    if (req->mConnection) {
        nsLoadFlags loadAttributes = nsIChannel::LOAD_NORMAL;
        req->mConnection->GetLoadAttributes(&loadAttributes);

        nsCOMPtr<nsIInterfaceRequestor> callbacks;
        req->mConnection->GetNotificationCallbacks(getter_AddRefs(callbacks));
        mTransport->SetNotificationCallbacks(callbacks,
                         (loadAttributes & nsIChannel::LOAD_BACKGROUND));
    }

    mOnStopDone = PR_FALSE;

    rv = NS_AsyncWriteFromStream(
            getter_AddRefs(mCurrentWriteRequest),
            mTransport, stream, 0, mRequestBuffer.Length(), 0, 
            this, NS_STATIC_CAST(nsIRequest*, req->mConnection));

    NS_RELEASE(req);

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPPipelinedRequest::OnStartRequest(nsIRequest *request, nsISupports* aContext)
{
    LOG(("nsHTTPRequest [this=%x]. Starting to write data to the server.\n",
        this));
    // this needs to propogate up if mInputStream was set...
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPPipelinedRequest::OnStopRequest(nsIRequest *request, nsISupports* aContext,
                                      nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv;
    nsCOMPtr<nsISocketTransport> trans = do_QueryInterface(mTransport, &rv);
    
    nsHTTPRequest * req =(nsHTTPRequest *) mRequests->ElementAt(0);

    rv = aStatus;

    LOG(("\nnsHTTPRequest::OnStopRequest() [this=%x], aStatus=%u\n",
        this, aStatus));

    if (NS_SUCCEEDED(rv)) {
        PRBool isAlive = PR_TRUE;

        if (!mListener && trans)
            trans->IsAlive(0, &isAlive);

        if (isAlive) {
            //
            // Write the input stream data to the server...
            //
            if (mInputStream) {
                LOG(("nsHTTPRequest [this=%x]. "
                     "Writing PUT/POST data to the server.\n", this));

                rv = NS_AsyncWriteFromStream(
                        getter_AddRefs(mCurrentWriteRequest),
                        mTransport, mInputStream, 0, 0, 0,
                        this, NS_STATIC_CAST(nsIRequest*, req->mConnection));

                /* the mInputStream is released below... */
            }
            //
            // Prepare to receive the response...
            //
            else {
                LOG(("nsHTTPRequest [this=%p]. "
                     "Finished writing request to server." 
                     "\tStatus: %x\n", this, aStatus));

                if (mListener == nsnull) {
                    nsHTTPResponseListener* pListener = new nsHTTPServerListener(
                            req->mConnection, 
                            mHandler, 
                            this,
                            req->mDoingProxySSLConnect);
                    if (pListener) {
                        NS_ADDREF(pListener);
                        rv = mTransport->AsyncRead(pListener, aContext, 0, (PRUint32) -1, 0,
                                                   getter_AddRefs(mCurrentReadRequest));
                        mListener  = pListener;
                        NS_RELEASE(pListener);
                    }
                    else
                        rv = NS_ERROR_OUT_OF_MEMORY;
                }

                mOnStopDone = PR_TRUE;
                // write again to see if anything else is queued up
                WriteRequest(mInputStream);
            }
        }
        else
            rv = NS_ERROR_FAILURE;

    }

    //
    // An error occurred when trying to write the request to the server!
    //
    else {
        LOG(("nsHTTPRequest [this=%p]. Error writing request to server."
             "\tStatus: %x\n", this, aStatus));
        rv = aStatus;
    }

    //
    // An error occurred...  Finish the transaction and notify the consumer
    // of the failure...
    //
    if (NS_FAILED(rv)) {
        if (mTotalProcessed == 0 && mAttempts == 0  && mTotalWritten) {
            // the pipeline just started - we still can attempt to recover

            PRUint32 wasKeptAlive = 0;
            nsresult channelStatus = NS_OK;

            if (trans)
                trans->GetReuseCount(&wasKeptAlive);

            req->mConnection->GetStatus(&channelStatus);

            LOG(("nsHTTPRequest::OnStopRequest() [this=%p]. wasKeptAlive=%d, channelStatus=%x\n",
                this, wasKeptAlive, channelStatus));

            if (wasKeptAlive && NS_SUCCEEDED(channelStatus)) {
                mMustCommit = PR_TRUE;
                mAttempts++;
                mTotalWritten = 0;

                mHandler->ReleaseTransport(mTransport, 
                        nsIHTTPProtocolHandler::DONTRECORD_CAPABILITIES, PR_TRUE);
                mTransport = null_nsCOMPtr();
                mCurrentWriteRequest = 0;
                mCurrentReadRequest = 0;

                mOnStopDone = PR_TRUE;
                rv = WriteRequest(mInputStream);
            
                if (NS_SUCCEEDED(rv)) {
                    NS_IF_RELEASE(req);
                    return rv;
                }
            }
        }

        while (req) {
            nsCOMPtr<nsIStreamListener> consumer;
            req->mConnection->GetResponseDataListener(getter_AddRefs(consumer));
            req->mConnection->ResponseCompleted(consumer, rv, nsnull);

            // Notify the HTTPChannel that the request has finished

            if (mTransport) {
                nsITransport *p = mTransport;
                mTransport = 0;
                mCurrentWriteRequest = 0;
                mCurrentReadRequest = 0;

                mHandler->ReleaseTransport(p, nsIHTTPProtocolHandler::DONTRECORD_CAPABILITIES);
            }

            NS_IF_RELEASE(req);
            req = nsnull;
        
            if (NS_SUCCEEDED(AdvanceToNextRequest()))
                GetCurrentRequest(&req);
        }
    }
 
    NS_IF_RELEASE(req);

    //
    // These resouces are no longer needed...
    //
    // mRequestBuffer.Truncate();
    mInputStream = null_nsCOMPtr();
    mOnStopDone = PR_TRUE;

    if (NS_FAILED(rv) && !mListener)
        mHandler->ReleasePipelinedRequest(this);

    return rv;
}

nsresult
nsHTTPPipelinedRequest::RestartRequest(PRUint32 aType)
{
    nsresult rval = NS_ERROR_FAILURE;

    LOG(("nsHTTPPipelinedRequest::RestartRequest() [this=%p], mTotalProcessed=%u\n",
        this, mTotalProcessed));

    if (aType == REQUEST_RESTART_SSL) {
        mTotalWritten = 0;
        mMustCommit   = PR_TRUE;
        // We don't have to do it, as this listener should get 
        // data on the second stage of proxy SSL connection as far
        // mListener     =  nsnull;

        // in case of SSL proxies we can't pipeline
        nsHTTPRequest * req =(nsHTTPRequest *) mRequests->ElementAt(0);
        req->mDoingProxySSLConnect = PR_FALSE;
        NS_RELEASE(req);
        //
        // XXX/ruslan: need to tell the transport to switch into SSL mode
        //
        nsCOMPtr<nsISupports> securityInfo;
        rval = mTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
        if (NS_FAILED(rval)) return rval;

        nsCOMPtr<nsISSLSocketControl> sslControl(do_QueryInterface(securityInfo, &rval));
        if (NS_FAILED(rval)) return rval;

        printf(">>> Doing SSL ProxyStepUp\n");
        rval = sslControl->ProxyStepUp();
        if (NS_FAILED(rval)) return rval;

        return WriteRequest(mInputStream);
    }

    if (mTotalProcessed == 0) {
        nsresult rv;
        // the pipeline just started - we still can attempt to recover

        nsCOMPtr<nsISocketTransport> trans = do_QueryInterface(mTransport, &rv);
        PRUint32 wasKeptAlive = 0;

        if (trans)
            trans->GetReuseCount(&wasKeptAlive);

        nsresult channelStatus = NS_OK;

        nsHTTPRequest * req = nsnull;
        GetCurrentRequest(&req);

        if (req) {
            if (req->mConnection)
                req->mConnection->GetStatus(&channelStatus);

            NS_RELEASE(req);
        }

        LOG(("nsHTTPPipelinedRequest::RestartRequest() [this=%p], wasKepAlive=%u, "
            "mAttempts=%d, mOnStopDone=%d\n",
            this, wasKeptAlive, mAttempts, mOnStopDone));

        if (wasKeptAlive && mAttempts == 0 && NS_SUCCEEDED(channelStatus)) {
            rval = NS_OK;
            mListener = nsnull;

            if (mOnStopDone) {
                mTotalWritten = 0;
                mAttempts++;
                mMustCommit = PR_TRUE;

                mHandler->ReleaseTransport(mTransport, 
                        nsIHTTPProtocolHandler::DONTRECORD_CAPABILITIES, PR_TRUE);
                mTransport = null_nsCOMPtr();
                mCurrentWriteRequest = 0;
                mCurrentReadRequest = 0;

                rval = WriteRequest(mInputStream);
            }
        }
    }
    return rval;
}

nsresult
nsHTTPPipelinedRequest::GetName(PRUnichar* *result)
{
    if (mCurrentReadRequest)
        return mCurrentReadRequest->GetName(result);

    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsHTTPPipelinedRequest::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;

    if (mCurrentReadRequest)
        rv = mCurrentReadRequest->IsPending(result);
    else
        *result = PR_FALSE; 

  return rv;
}

nsresult
nsHTTPPipelinedRequest::Cancel(nsresult status)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(NS_FAILED(status), "Can't cancel with a sucessful status");

    if (mCurrentReadRequest)
        rv = mCurrentReadRequest->Cancel(status);

    if (mCurrentWriteRequest)
        (void) mCurrentWriteRequest->Cancel(status);

    return rv;
}

nsresult
nsHTTPPipelinedRequest::Suspend(void)
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mCurrentReadRequest)
        rv = mCurrentReadRequest->Suspend();

    if (mCurrentWriteRequest)
        (void) mCurrentWriteRequest->Suspend();

    return rv;
}

nsresult
nsHTTPPipelinedRequest::Resume()
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mCurrentReadRequest)
        rv = mCurrentReadRequest->Resume();

    if (mCurrentWriteRequest)
        (void) mCurrentWriteRequest->Resume();

    return rv;
}

static PRUint32 sLongestPipeline = 0;

nsresult
nsHTTPPipelinedRequest::AddToPipeline(nsHTTPRequest *aRequest)
{
    if (!mRequests)
        return NS_ERROR_FAILURE;

    PRUint32 count = 0;

    mRequests->Count(&count);

    if (count > 0) {
        if (aRequest->mInputStream)
            return NS_ERROR_FAILURE;

        if (mMustCommit)
            return NS_ERROR_FAILURE;
    }

    if (aRequest->mInputStream) {
        if (count > 0)
            return NS_ERROR_FAILURE;
        else {
            mInputStream = aRequest->mInputStream;
            mMustCommit = PR_TRUE;
        }
    }

    if (!(mCapabilities &(nsIHTTPProtocolHandler::ALLOW_PROXY_PIPELINING|nsIHTTPProtocolHandler::ALLOW_PIPELINING)))
        mMustCommit = PR_TRUE;

    if (aRequest->mDoingProxySSLConnect)
        mMustCommit = PR_TRUE;

    aRequest->mPipelinedRequest  = this;
    mRequests->AppendElement(aRequest);

    if (sLongestPipeline < count + 1)
        sLongestPipeline = count + 1;

    LOG(("nsHTTPPipelinedRequest::AddToPipeline() [this=%p]"
        " count=%u, longest pipeline=%u\n",
        this, count + 1, sLongestPipeline));

    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetCurrentRequest(nsHTTPRequest **aReq)
{
    NS_ENSURE_ARG_POINTER(aReq);

    PRUint32 count = 0;
    mRequests->Count(&count);

    *aReq = nsnull;

    if (count == 0)
        return NS_ERROR_FAILURE;

    *aReq = (nsHTTPRequest *) mRequests->ElementAt(0);
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetRequestCount(PRUint32 *aCount)
{
    NS_ENSURE_ARG_POINTER(aCount);

    PRUint32 count = 0;
    mRequests->Count(&count);

    *aCount = count;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetMustCommit(PRBool *aVal)
{
    NS_ENSURE_ARG_POINTER(aVal);
    *aVal = mMustCommit;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::SetMustCommit(PRBool aVal)
{
    mMustCommit = aVal;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::AdvanceToNextRequest()
{
    PRUint32 count = 0;
    nsHTTPRequest *req;

    mRequests->Count(&count);

    if (count == 0)
        return NS_ERROR_FAILURE;

    req =(nsHTTPRequest *) mRequests->ElementAt(0);
    if (req) {
        req->mPipelinedRequest = nsnull;

        mRequests->RemoveElement(req);
        NS_RELEASE(req);

        mTotalProcessed++;
    }

    mRequests->Count(&count);

    if (count == 0)
        return NS_ERROR_FAILURE;

    req =(nsHTTPRequest *) mRequests->ElementAt(0);

    if (req) {
        // XXX bug 52492:  For some reason, mTransport is often null
        // on some machines, but not on others.  Check for null to avoid
        // topcrash, although we really shouldn't need this.
        NS_ASSERTION(mTransport, "mTransport null in AdvanceToNextRequest");
        nsLoadFlags flags = nsIChannel::LOAD_NORMAL;
        req->mConnection->GetLoadAttributes(&flags);

        nsCOMPtr<nsIInterfaceRequestor> callbacks;
        mTransport->GetNotificationCallbacks(getter_AddRefs(callbacks));
        mTransport->SetNotificationCallbacks(callbacks,
                                             (flags & nsIChannel::LOAD_BACKGROUND));
        NS_RELEASE(req);
    }

    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetSameRequest(const char *host, PRInt32 port, PRBool *aSame)
{
    if (host == NULL || aSame == NULL)
        return NS_ERROR_NULL_POINTER;

    if (port == mPort && !PL_strcasecmp(host, mHost))
        *aSame = PR_TRUE;
    else
        *aSame = PR_FALSE;
    return NS_OK;
}

nsresult
nsHTTPRequest::GetUploadStream(nsIInputStream **aUploadStream)
{
    NS_ENSURE_ARG_POINTER(aUploadStream);
    
    // until we are in process of SSL connect no upload stream is available 
    if (mDoingProxySSLConnect) 
        *aUploadStream = nsnull;
    else
        *aUploadStream = mInputStream;
    NS_IF_ADDREF(*aUploadStream);
    return NS_OK;

}

nsresult
nsHTTPRequest::SetUploadStream(nsIInputStream *aUploadStream)
{
    mInputStream = aUploadStream;
    return NS_OK;
}
