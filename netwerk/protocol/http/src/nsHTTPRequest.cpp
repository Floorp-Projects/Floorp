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
 * Contributor(s): 
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
#include "nsIPSMSocketInfo.h"
#include "plstr.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
static NS_DEFINE_CID(kHTTPHandlerCID,   NS_IHTTPHANDLER_CID);

extern nsresult DupString(char* *o_Dest, const char* i_Src);

nsHTTPRequest::nsHTTPRequest(nsIURI* i_URL, 
        nsHTTPHandler* i_Handler, 
        PRUint32 bufferSegmentSize, 
        PRUint32 bufferMaxSize)
    :
    mBufferSegmentSize(bufferSegmentSize),
    mBufferMaxSize(bufferMaxSize),
    mPipelinedRequest(nsnull),
    mVersion(HTTP_ONE_ZERO),
    mKeepAliveTimeout(0),
    mRequestSpec(0),
    mHandler(i_Handler),
    mAbortStatus(NS_OK),
    mHeadersFormed(PR_FALSE),
    mPort(-1),
    mDoingProxySSLConnect(PR_FALSE),
    mProxySSLConnectAllowed(PR_FALSE)
{   
    NS_INIT_REFCNT();

    NS_ASSERTION(i_URL, "No URL for the request!!");
    mURI = do_QueryInterface(i_URL);

    mMethod = nsHTTPAtoms::Get;

#if defined(PR_LOGGING)
  nsXPIDLCString urlCString; 
    mURI->GetSpec(getter_Copies(urlCString));
  
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
("Creating nsHTTPRequest [this=%x] for URI: %s.\n", 
           this,(const char *)urlCString));
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
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("Deleting nsHTTPRequest [this=%x].\n", this));

    CRTFREEIF(mRequestSpec);
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTTPRequest, nsIRequest)

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsHTTPRequest::GetName(PRUnichar* *result)
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

nsresult nsHTTPRequest::Clone(const nsHTTPRequest* *o_Request) const
{
    return NS_ERROR_FAILURE;
}
                        
nsresult nsHTTPRequest::SetMethod(nsIAtom * i_Method)
{
    mMethod = i_Method;
    return NS_OK;
}

nsIAtom * nsHTTPRequest::GetMethod(void) const
{
    nsIAtom * ap = mMethod;
    NS_ADDREF(ap);

    return mMethod;
}
                        
nsresult nsHTTPRequest::SetPriority()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsHTTPRequest::GetPriority()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult nsHTTPRequest::SetHeader(nsIAtom* i_Header, const char* i_Value)
{
    return mHeaders.SetHeader(i_Header, i_Value);
}

nsresult nsHTTPRequest::GetHeader(nsIAtom* i_Header, char* *o_Value)
{
    return mHeaders.GetHeader(i_Header, o_Value);
}


nsresult nsHTTPRequest::SetHTTPVersion(PRUint32  i_Version)
{
    mVersion = i_Version;
    return NS_OK;
}

nsresult nsHTTPRequest::GetHTTPVersion(PRUint32 * o_Version) 
{
    *o_Version = mVersion;
    return NS_OK;
}


nsresult nsHTTPRequest::SetConnection(nsHTTPChannel*  i_Connection)
{
    mConnection = i_Connection;
    return NS_OK;
}

nsresult nsHTTPRequest::GetConnection(nsHTTPChannel** o_Connection)
{
    nsresult rv = NS_OK;

    if (o_Connection)
        *o_Connection = mConnection;
    else
        rv = NS_ERROR_NULL_POINTER;

    return rv;
}

nsresult nsHTTPRequest::SetTransport(nsIChannel * aTransport)
{
    return NS_OK;
}

nsresult nsHTTPRequest::GetTransport(nsIChannel **aTransport)
{
    if (mPipelinedRequest)
        return mPipelinedRequest->GetTransport(aTransport);
    else
        *aTransport = nsnull;

    return NS_OK;
}

nsresult nsHTTPRequest::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator(aResult);
}

nsresult
nsHTTPRequest::SetOverrideRequestSpec(const char* i_Spec)
{
    CRTFREEIF(mRequestSpec);
    
    if (i_Spec)
    {
        // proxy case
        if (!PL_strcasecmp(mSpec, "https") && mProxySSLConnectAllowed)
            mDoingProxySSLConnect = PR_TRUE;
    }
    
    return DupString(&mRequestSpec, i_Spec);
}

nsresult
nsHTTPRequest::GetOverrideRequestSpec(char** o_Spec)
{
    return DupString(o_Spec, mRequestSpec);
}

nsresult
nsHTTPRequest::formHeaders(PRUint32 capabilities)
{
    if (mHeadersFormed)
        return NS_OK;

    // Send Host header by default
    if (HTTP_ZERO_NINE != mVersion)
    {
        if (-1 != mPort)
        {
            char* tempHostPort = 
                PR_smprintf(PL_strchr(mHost, ':') ? "[%s]:%d" : "%s:%d",
                            (const char*)mHost, mPort);
            if (tempHostPort)
            {
                SetHeader(nsHTTPAtoms::Host, tempHostPort);
                PR_smprintf_free(tempHostPort);
                tempHostPort = 0;
            }
        }
        else
            SetHeader(nsHTTPAtoms::Host, mHost);
    }

    // Add the user-agent 
    nsresult rv = NS_OK;
    NS_WITH_SERVICE(nsIHTTPProtocolHandler, httpHandler, kHTTPHandlerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString ua;
    if (NS_SUCCEEDED(httpHandler->GetUserAgent(getter_Copies(ua))))
    {
        nsCAutoString uaString; uaString.AssignWithConversion(NS_STATIC_CAST(const PRUnichar*, ua));
        SetHeader(nsHTTPAtoms::User_Agent, uaString.GetBuffer());
    }

    PRUint32 loadAttributes;
    mConnection->GetLoadAttributes(&loadAttributes);
    
    if (loadAttributes &(nsIChannel::FORCE_VALIDATION | 
                nsIChannel::FORCE_RELOAD))
    {

        // We need to send 'Pragma:no-cache' to inhibit proxy caching even if
        // no proxy is configured since we might be talking with a transparent
        // proxy, i.e. one that operates at the network level.  See bug #14772
        SetHeader(nsHTTPAtoms::Pragma, "no-cache");
    }

    if (loadAttributes & nsIChannel::FORCE_RELOAD)
    {
        // If doing a reload, force end-to-end 
        SetHeader(nsHTTPAtoms::Cache_Control, "no-cache");
    }
    else
    if (loadAttributes & nsIChannel::FORCE_VALIDATION)
    {
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

    nsXPIDLCString acceptLanguages;
    // Add the Accept-Language header
    rv = httpHandler->GetAcceptLanguages(getter_Copies(acceptLanguages));
    
    if (NS_SUCCEEDED(rv))
    {
        if (acceptLanguages && *acceptLanguages)
            SetHeader(nsHTTPAtoms::Accept_Language, acceptLanguages);
    }

    nsXPIDLCString acceptEncodings;
    rv = httpHandler->GetAcceptEncodings(getter_Copies(acceptEncodings));
    
    if (NS_SUCCEEDED(rv))
    {
        if (acceptEncodings && *acceptEncodings)
            SetHeader(nsHTTPAtoms::Accept_Encoding, acceptEncodings);
    }

    if (capabilities &(nsIHTTPProtocolHandler::ALLOW_KEEPALIVE|nsIHTTPProtocolHandler::ALLOW_PROXY_KEEPALIVE))
    {
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

    if (mDoingProxySSLConnect)
    {
        requestBuffer->Append("CONNECT ");
        requestBuffer->Append(mHost);
        requestBuffer->Append(":");

        char tmp[20];
        sprintf(tmp, "%u",(mPort == -1) ? 
            ((!PL_strcasecmp(mSpec, "https")) ? 443 : 80) 
                    : mPort);
        requestBuffer->Append(tmp);
    }
    else
    {
        mMethod->ToString(methodString);
        cp.AssignWithConversion(methodString);

        requestBuffer->Append(cp);
        requestBuffer->Append(" ");

        // Request spec gets set for proxied cases-
        if (!mRequestSpec)
        {
            rv = mURI->GetPath(getter_Copies(autoBuffer));
            requestBuffer->Append(autoBuffer);
        }
        else
            requestBuffer->Append(mRequestSpec);
    
        //if (mRequestSpec) 
        /*
        else
        {
            if (aIsProxied) {
                rv = mURI->GetSpec(getter_Copies(autoBuffer));
            } else {
                rv = mURI->GetPath(getter_Copies(autoBuffer));
            }
            mRequestBuffer.Append(autoBuffer);
        }
        */
    
        //Trim off the # portion if any...
        int refLocation = requestBuffer->RFind("#");
    
        if (-1 != refLocation)
            requestBuffer->Truncate(refLocation);
    }

    char * httpVersion = " HTTP/1.0" CRLF;

    switch (mVersion)
    {
        case HTTP_ZERO_NINE:
            httpVersion = " HTTP/0.9"CRLF;
            break;
        case HTTP_ONE_ONE:
            if (!(capabilities & NS_STATIC_CAST(unsigned long, nsIHTTPProtocolHandler::DONTALLOW_HTTP11)))
                httpVersion = " HTTP/1.1"CRLF;
    }

    requestBuffer->Append(httpVersion);

    //
    // Write the request headers, if any...
    //
    // ie. field-name ":" [field-value] CRLF
    //
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = mHeaders.GetEnumerator(getter_AddRefs(enumerator));

    if (NS_SUCCEEDED(rv))
    {
        PRBool bMoreHeaders;
        nsCOMPtr<nsISupports>   item;
        nsCOMPtr<nsIHTTPHeader> header;
        nsCOMPtr<nsIAtom>       headerAtom;

        enumerator->HasMoreElements(&bMoreHeaders);
        while (bMoreHeaders)
        {
            enumerator->GetNext(getter_AddRefs(item));
            header = do_QueryInterface(item);

            NS_ASSERTION(header, "Bad HTTP header.");
            
            if (header)
            {
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
    if (!mInputStream)
        requestBuffer->Append(CRLF);

    return NS_OK;
}

static  PRUint32 sPipelinedRequestCreated = 0;
static  PRUint32 sPipelinedRequestDeleted = 0;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsHTTPPipelinedRequest, nsIStreamObserver)

nsHTTPPipelinedRequest::nsHTTPPipelinedRequest(nsHTTPHandler* i_Handler, const char *host, PRInt32 port, PRUint32 capabilities)
    :   mCapabilities(capabilities),
        mAttempts(0),
        mBufferSegmentSize(0),
        mBufferMaxSize(0),
        mMustCommit(PR_FALSE),
        mTotalWritten(0),
        mTotalProcessed(0),
    	mHandler(i_Handler),
        mPort(port),
        mListener(nsnull),
        mOnStopDone(PR_TRUE)
{   
    NS_INIT_REFCNT();

    mHost = host;    
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,("Creating nsHTTPPipelinedRequest [this=%x], created=%d, deleted=%d\n",
                this, ++sPipelinedRequestCreated, sPipelinedRequestDeleted));

    NS_NewISupportsArray(getter_AddRefs(mRequests));

}
    

nsHTTPPipelinedRequest::~nsHTTPPipelinedRequest()
{
    PRUint32 count = 0;
    PRInt32  index;

    PR_LOG(gHTTPLog, PR_LOG_DEBUG,("Deleting nsHTTPPipelinedRequest [this=%x], created=%d, deleted=%d\n",
                this, sPipelinedRequestCreated, ++sPipelinedRequestDeleted));

    if (mRequests)
    {
        mRequests->Count(&count);

        if (count > 0)
        {
            for (index =(PRInt32) count - 1; index >= 0; --index)
            {
                nsHTTPRequest * req =(nsHTTPRequest *) mRequests->ElementAt(index);
                if (req != NULL)
                {
                    req->mPipelinedRequest = nsnull;
                    mRequests->RemoveElement(req);
                    NS_RELEASE(req);
                }
            }
        }
    } /* mRequests */
}

nsresult
nsHTTPPipelinedRequest::SetTransport(nsIChannel * aTransport)
{
    mTransport = aTransport;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetTransport(nsIChannel **aTransport)
{
    if (aTransport == NULL)
        return NS_ERROR_NULL_POINTER;

    *aTransport = mTransport;
    NS_IF_ADDREF(*aTransport);

    return NS_OK;
}

// Finally our own methods...

nsresult
nsHTTPPipelinedRequest::WriteRequest(nsIInputStream* iRequestStream)
{
    nsresult rv = NS_OK;

    PRUint32 count = 0;
    PRUint32 index;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS,("nsHTTPPipelinedRequest::WriteRequest()[%x], mOnStopDone=%d, mTransport=%x\n", this, mOnStopDone, mTransport));

    if (!mRequests)
        return NS_ERROR_FAILURE;

    if (!mOnStopDone)
        return NS_OK;

    mRequests->Count(&count);

    if (count == 0 || mTotalWritten - mTotalProcessed >= count)
        return NS_OK;

    if (mAttempts > 1)
        return NS_ERROR_FAILURE;

    nsHTTPRequest * req =(nsHTTPRequest *) mRequests->ElementAt(0);

    req->GetUploadStream(getter_AddRefs(mInputStream));

    if (iRequestStream && !mInputStream)
        mInputStream = iRequestStream;

    if (!mTransport)
    {
        PRUint32 tMode = mAttempts ? TRANSPORT_OPEN_ALWAYS : TRANSPORT_REUSE_ALIVE;
        if (mInputStream)
            tMode &= ~(TRANSPORT_REUSE_ALIVE);

        rv = mHandler->RequestTransport(req->mURI, req->mConnection, mBufferSegmentSize, mBufferMaxSize, 
                                           getter_AddRefs(mTransport), tMode);

        if (NS_FAILED(rv))
        {
            NS_RELEASE(req);
            return rv;
        }
    }
    
    NS_RELEASE(req);

    for (index = mTotalWritten - mTotalProcessed; index < count; index++)
    {
        req =(nsHTTPRequest *) mRequests->ElementAt(index);
        req->formHeaders(mCapabilities);
        NS_RELEASE(req);
    }

    mRequestBuffer.Truncate();

    for (index = mTotalWritten - mTotalProcessed; index < count; index++)
    {
        req =(nsHTTPRequest *) mRequests->ElementAt(index);
        req->formBuffer(&mRequestBuffer, mCapabilities);
        if (index == 0)
            mTransport->SetNotificationCallbacks(req->mConnection);

        NS_RELEASE(req);
        mTotalWritten++;
    }

    //
    // Build up the request into mRequestBuffer...
    //
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("\nnsHTTPRequest::Build() [this=%x].\tWriting Request:\n"
            "=== Start\n%s=== End\n",
            this, mRequestBuffer.GetBuffer()));

    //
    // Create a string stream to feed to the socket transport.  This stream
    // contains only the request.  Any PUT/POST data will be sent in a second
    // AsyncWrite to the server..
    //
    nsCOMPtr<nsISupports>    result;
    nsCOMPtr<nsIInputStream> stream;

    rv = NS_NewCharInputStream(getter_AddRefs(result), mRequestBuffer.GetBuffer());
    if (NS_FAILED(rv)) return rv;

    stream = do_QueryInterface(result, &rv);
    if (NS_FAILED(rv)) return rv;

    //
    // Write the request to the server.  
    //
    rv = mTransport->SetTransferCount(mRequestBuffer.Length());
    
    if (NS_FAILED(rv)) return rv;
    req =(nsHTTPRequest *) mRequests->ElementAt(0);

    mOnStopDone = PR_FALSE;
    rv = mTransport->AsyncWrite(stream, this,(nsISupports*)(nsIRequest*)req->mConnection);
    NS_RELEASE(req);

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPPipelinedRequest::OnStartRequest(nsIChannel* channel, nsISupports* i_Context)
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("nsHTTPRequest [this=%x]. Starting to write data to the server.\n",
            this));
    // this needs to propogate up if mInputStream was set...
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPPipelinedRequest::OnStopRequest(nsIChannel* channel, nsISupports* i_Context,
                                      nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv;
    nsCOMPtr<nsISocketTransport> trans = do_QueryInterface(mTransport, &rv);
    
    nsHTTPRequest * req =(nsHTTPRequest *) mRequests->ElementAt(0);

    rv = aStatus;

    PR_LOG(gHTTPLog, PR_LOG_DEBUG,("\nnsHTTPRequest::OnStopRequest() [this=%x], aStatus=%u\n", this, aStatus));

    if (NS_SUCCEEDED(rv))
    {
        PRBool isAlive = PR_TRUE;

        if (!mListener && trans)
            trans->IsAlive(0, &isAlive);

        if (isAlive)
        {
            //
            // Write the input stream data to the server...
            //
            if (mInputStream)
            {
                PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
                       ("nsHTTPRequest [this=%x]. "
                        "Writing PUT/POST data to the server.\n", this));

                rv = mTransport->AsyncWrite(mInputStream, this, 
                                            (nsISupports*)(nsIRequest*)req->mConnection);

                /* the mInputStream is released below... */
            }
            //
            // Prepare to receive the response...
            //
            else
            {
                PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
                       ("nsHTTPRequest [this=%x]. "
                        "Finished writing request to server." 
                        "\tStatus: %x\n", this, aStatus));

                if (mListener == nsnull)
                {
                    nsHTTPResponseListener* pListener = new nsHTTPServerListener(
                            req->mConnection, 
                            mHandler, 
                            this,
                            req->mDoingProxySSLConnect);
                    if (pListener)
                    {
                        NS_ADDREF(pListener);
                        rv = mTransport->AsyncRead(pListener, i_Context);
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
            rv = NS_ERROR_FAILURE; /* isAlive */

    } /* NS_SUCCEEDED */

    //
    // An error occurred when trying to write the request to the server!
    //
    else
    {
        PR_LOG(gHTTPLog, PR_LOG_ERROR, 
               ("nsHTTPRequest [this=%x]. Error writing request to server."
                "\tStatus: %x\n", this, aStatus));
        rv = aStatus;
    }

    //
    // An error occurred...  Finish the transaction and notify the consumer
    // of the failure...
    //
    if (NS_FAILED(rv))
    {
        if (mTotalProcessed == 0 && mAttempts == 0  && mTotalWritten)
        {
            // the pipeline just started - we still can attempt to recover

            PRUint32 wasKeptAlive = 0;
            nsresult channelStatus = NS_OK;

            if (trans)
                trans->GetReuseCount(&wasKeptAlive);

            req->mConnection->GetStatus(&channelStatus);

            PR_LOG(gHTTPLog, PR_LOG_DEBUG,("nsHTTPRequest::OnStopRequest() [this=%x]. wasKeptAlive=%d, channelStatus=%x\n", this, wasKeptAlive, channelStatus));

            if (wasKeptAlive && NS_SUCCEEDED(channelStatus))
            {
                mMustCommit = PR_TRUE;
                mAttempts++;
                mTotalWritten = 0;

                mHandler->ReleaseTransport(mTransport, 
                        nsIHTTPProtocolHandler::DONTRECORD_CAPABILITIES, PR_TRUE);
                mTransport = null_nsCOMPtr();
                mOnStopDone = PR_TRUE;
                rv = WriteRequest(mInputStream);

            
                if (NS_SUCCEEDED(rv))
                {
                    NS_IF_RELEASE(req);
                    return rv;
                }
            }
        }

        while (req)
        {
            nsCOMPtr<nsIStreamListener> consumer;
            req->mConnection->GetResponseDataListener(getter_AddRefs(consumer));
            req->mConnection->ResponseCompleted(consumer, rv, nsnull);

            // Notify the HTTPChannel that the request has finished

            if (mTransport)
            {
                nsIChannel *p = mTransport;
                mTransport = null_nsCOMPtr();

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

    PR_LOG(gHTTPLog, PR_LOG_DEBUG,("nsHTTPPipelinedRequest::RestartRequest() [this=%x], mTotalProcessed=%u\n", this, mTotalProcessed));

    if (aType == REQUEST_RESTART_SSL)
    {
        mTotalWritten = 0;
        mMustCommit   = PR_TRUE;
        mListener     =  nsnull;

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

        nsCOMPtr<nsIPSMSocketInfo> psmSocketInfo = do_QueryInterface(securityInfo, &rval);
        if (NS_FAILED(rval)) return rval;

        rval = psmSocketInfo->ProxyStepUp();
        if (NS_FAILED(rval)) return rval;

        return WriteRequest(mInputStream);
    }

    if (mTotalProcessed == 0)
    {
        nsresult rv;
        // the pipeline just started - we still can attempt to recover

        nsCOMPtr<nsISocketTransport> trans = do_QueryInterface(mTransport, &rv);
        PRUint32 wasKeptAlive = 0;

        if (trans)
            trans->GetReuseCount(&wasKeptAlive);

        nsresult channelStatus = NS_OK;

        nsHTTPRequest * req = nsnull;
        GetCurrentRequest(&req);

        if (req)
        {
            if (req->mConnection)
                req->mConnection->GetStatus(&channelStatus);

            NS_RELEASE(req);
        }

        PR_LOG(gHTTPLog, PR_LOG_DEBUG,("nsHTTPPipelinedRequest::RestartRequest() [this=%x], wasKepAlive=%u, mAttempts=%d, mOnStopDone=%d\n", this, wasKeptAlive, mAttempts, mOnStopDone));

        if (wasKeptAlive && mAttempts == 0 && NS_SUCCEEDED(channelStatus))
        {
            rval = NS_OK;
            mListener = nsnull;

            if (mOnStopDone)
            {
                mTotalWritten = 0;
                mAttempts++;
                mMustCommit = PR_TRUE;

                mHandler->ReleaseTransport(mTransport, 
                        nsIHTTPProtocolHandler::DONTRECORD_CAPABILITIES, PR_TRUE);
                mTransport = null_nsCOMPtr();

                rval = WriteRequest(mInputStream);
            }
        }
    }
    return rval;
}

nsresult
nsHTTPPipelinedRequest::GetName(PRUnichar* *result)
{
    if (mTransport)
        return mTransport->GetName(result);

    NS_NOTREACHED("nsHTTPPipelinedRequest::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsHTTPPipelinedRequest::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;

    if (mTransport)
        rv = mTransport->IsPending(result);
    else
        *result = PR_FALSE; 

  return rv;
}

nsresult
nsHTTPPipelinedRequest::Cancel(nsresult status)
{
    nsresult rv = NS_OK;

    if (mTransport)
        rv = mTransport->Cancel(status);

    return rv;
}

nsresult
nsHTTPPipelinedRequest::Suspend(void)
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mTransport)
        rv = mTransport->Suspend();
    return rv;
}

nsresult
nsHTTPPipelinedRequest::Resume()
{
    nsresult rv = NS_ERROR_FAILURE;

    if (mTransport)
        rv = mTransport->Resume();
    return rv;
}

static  PRUint32 sLongestPipeline = 0;

nsresult
nsHTTPPipelinedRequest::AddToPipeline(nsHTTPRequest *aRequest)
{
    if (!mRequests)
        return NS_ERROR_FAILURE;

    PRUint32 count = 0;

    mRequests->Count(&count);

    if (count > 0)
    {
        if (aRequest->mInputStream)
            return NS_ERROR_FAILURE;

        if (mMustCommit)
            return NS_ERROR_FAILURE;
    }

    if (aRequest->mInputStream)
    {
        if (count > 0)
            return NS_ERROR_FAILURE;
        else
        {
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

    if (mBufferSegmentSize < aRequest->mBufferSegmentSize)
        mBufferSegmentSize = aRequest->mBufferSegmentSize;

    if (mBufferMaxSize < aRequest->mBufferMaxSize)
        mBufferMaxSize = aRequest->mBufferMaxSize;

    if (sLongestPipeline < count + 1)
        sLongestPipeline = count + 1;

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
("nsHTTPPipelinedRequest::AddToPipeline() [this=%x]"
        " count=%u, longest pipeline=%u\n",
        this, count + 1, sLongestPipeline));

    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetCurrentRequest(nsHTTPRequest ** o_Req)
{
    PRUint32 count = 0;
    mRequests->Count(&count);

    if (o_Req == NULL)
        return NS_ERROR_NULL_POINTER;

    *o_Req = nsnull;

    if (count == 0)
        return NS_ERROR_FAILURE;

    *o_Req =(nsHTTPRequest *)mRequests->ElementAt(0);
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetRequestCount(PRUint32 * o_Count)
{
    PRUint32 count = 0;
    mRequests->Count(&count);

    if (o_Count == NULL)
        return NS_ERROR_NULL_POINTER;

    *o_Count = count;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetMustCommit(PRBool * o_Val)
{
    if (o_Val == NULL)
        return NS_ERROR_NULL_POINTER;

    *o_Val = mMustCommit;
    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::SetMustCommit(PRBool val)
{
    mMustCommit = val;
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
    if (req)
    {
        req->mPipelinedRequest = nsnull;

        mRequests->RemoveElement(req);
        NS_RELEASE(req);

        mTotalProcessed++;
    }

    mRequests->Count(&count);

    if (count == 0)
        return NS_ERROR_FAILURE;

    req =(nsHTTPRequest *) mRequests->ElementAt(0);

    if (req)
    {
        mTransport->SetNotificationCallbacks(req->mConnection);
        NS_RELEASE(req);
    }

    return NS_OK;
}

nsresult
nsHTTPPipelinedRequest::GetSameRequest(const char *host, PRInt32 port, PRBool * aSame)
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
nsHTTPRequest::GetUploadStream(nsIInputStream** o_UploadStream)
{
    if (!o_UploadStream)
        return NS_ERROR_NULL_POINTER;
    *o_UploadStream = mInputStream;
    
    NS_IF_ADDREF(*o_UploadStream);

    return NS_OK;

}

nsresult
nsHTTPRequest::SetUploadStream(nsIInputStream* i_UploadStream)
{
    mInputStream = i_UploadStream;

    return NS_OK;
}
