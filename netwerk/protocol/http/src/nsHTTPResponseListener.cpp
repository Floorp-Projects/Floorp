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
#include "nsHTTPRequest.h"
#include "nsIStreamListener.h"
#include "nsHTTPResponseListener.h"
#include "nsIChannel.h"
#include "nsISocketTransport.h"
#include "nsIInputStream.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponse.h"
#include "nsCRT.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"
#include "nsRepository.h"
#include "nsIByteArrayInputStream.h"

#include "nsHTTPAtoms.h"
#include "nsIHttpNotify.h"
#include "nsINetModRegEntry.h"
#include "nsIServiceManager.h"
#include "nsINetModuleMgr.h"
#include "nsIEventQueueService.h"

#include "nsXPIDLString.h" 

#include "nsIIOService.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID) ;

#if defined(PR_LOGGING) 
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

//
// This specifies the maximum allowable size for a server Status-Line
// or Response-Header.
//
static const int kMAX_HEADER_SIZE = 60000;

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID) ;

nsHTTPResponseListener::nsHTTPResponseListener(nsHTTPChannel *aChannel, 
        nsHTTPHandler *handler) 
           :mChannel(aChannel) , 
            mHandler(handler) 
{
  NS_INIT_REFCNT() ;

  // mChannel is not an interface pointer, so COMPtrs cannot be used :-(
  NS_ASSERTION(aChannel, "HTTPChannel is null.") ;
  NS_ADDREF(mChannel) ;

#if defined(PR_LOGGING) 
  nsCOMPtr<nsIURI> url;
  nsXPIDLCString urlCString; 

  aChannel->GetURI(getter_AddRefs(url)) ;
  if (url) {
    url->GetSpec(getter_Copies(urlCString)) ;
  }
  
  PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
("Creating nsHTTPResponseListener [this=%x] for URI: %s.\n", 
           this,(const char *) urlCString)) ;
#endif
}

nsHTTPResponseListener::~nsHTTPResponseListener() 
{
  // mChannel is not an interface pointer, so COMPtrs cannot be used :-(
  NS_IF_RELEASE(mChannel) ;
}


void nsHTTPResponseListener::SetListener(nsIStreamListener *aListener) 
{
  mResponseDataListener = aListener;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_THREADSAFE_ADDREF(nsHTTPResponseListener) 
NS_IMPL_THREADSAFE_RELEASE(nsHTTPResponseListener) 

NS_IMPL_QUERY_INTERFACE2(nsHTTPResponseListener, 
                         nsIStreamListener, 
                         nsIStreamObserver) ;



///////////////////////////////////////////////////////////////////////////////
//
// nsHTTPCacheListener Implementation
//
// This subclass of nsHTTPResponseListener processes responses from
// the cache.
//
///////////////////////////////////////////////////////////////////////////////
nsHTTPCacheListener::nsHTTPCacheListener(nsHTTPChannel* aChannel, nsHTTPHandler *handler) 
                   : nsHTTPResponseListener(aChannel, handler) 
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
          ("Creating nsHTTPCacheListener [this=%x].\n", this)) ;

}

nsHTTPCacheListener::~nsHTTPCacheListener() 
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
            ("Deleting nsHTTPCacheListener [this=%x].\n", this)) ;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPCacheListener::OnStartRequest(nsIChannel *aChannel,
                                    nsISupports *aContext) 
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,
            ("nsHTTPCacheListener::OnStartRequest [this=%x]\n", this)) ;
    mBodyBytesReceived = 0;
    
    // get and store the content length which will be used in ODA for computing
    // progress information. 
    aChannel->GetContentLength(&mContentLength) ;
    return mResponseDataListener->OnStartRequest(mChannel, aContext) ;
}

NS_IMETHODIMP
nsHTTPCacheListener::OnStopRequest(nsIChannel *aChannel, nsISupports *aContext,
                                   nsresult aStatus, const PRUnichar* aStatusArg) 
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,
            ("nsHTTPCacheListener::OnStopRequest [this=%x]\n", this)) ;

    //
    // Notify the channel that the response has finished.  Since there
    // is no socket transport involved nsnull is passed as the
    // transport...
    //
    nsresult rv = mChannel->ResponseCompleted(mResponseDataListener, aStatus, aStatusArg) ;
//    NS_IF_RELEASE(mChannel) ;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsHTTPCacheListener::OnDataAvailable(nsIChannel *aChannel,
                                     nsISupports *aContext,
                                     nsIInputStream *aStream,
                                     PRUint32 aSourceOffset,
                                     PRUint32 aCount) 
{
    nsresult channelStatus = NS_OK, rv = NS_OK;
    
    if (mChannel) 
        mChannel->GetStatus(&channelStatus) ;

    if (NS_FAILED(channelStatus)) // Canceled http channel
         return NS_OK;

    rv = mResponseDataListener->OnDataAvailable(mChannel, 
                                                aContext,
                                                aStream,
                                                aSourceOffset,
                                                aCount) ;
    if (NS_FAILED(rv)) return rv;


    mBodyBytesReceived += aCount;

    // Report progress
    rv = mChannel->ReportProgress(mBodyBytesReceived, mContentLength) ;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTTPResponseListener methods:

nsresult
nsHTTPCacheListener::FireSingleOnData(nsIStreamListener *aListener, 
                                      nsISupports *aContext) 
{
  NS_ASSERTION(0, "nsHTTPCacheListener::FireSingleOnData(...) "
                  "should never be called.") ;
 
  return NS_ERROR_FAILURE;
}

nsresult nsHTTPCacheListener::Abort() 
{
  NS_ASSERTION(0, "nsHTTPCachedResponseListener::Abort() "
                  "should never be called.") ;
 
  return NS_ERROR_FAILURE;
}


static NS_DEFINE_CID(kSupportsVoidCID, NS_SUPPORTS_VOID_CID) ;
static NS_DEFINE_IID(kSupportsVoidIID, NS_ISUPPORTSVOID_IID) ; 

////////////////////////////////////////////////////////////////////////////////
//
// nsHTTPServerListener Implementation
//
// This subclass of nsHTTPResponseListener processes responses from
// HTTP servers.
//
////////////////////////////////////////////////////////////////////////////////

nsHTTPServerListener::nsHTTPServerListener(nsHTTPChannel* aChannel, nsHTTPHandler *handler, nsHTTPPipelinedRequest * request, PRBool aDoingProxySSLConnect) 
                    : nsHTTPResponseListener(aChannel, handler) ,
                      mResponse(nsnull) ,
                      mFirstLineParsed(PR_FALSE) ,
                      mHeadersDone(PR_FALSE) ,
                      mSimpleResponse(PR_FALSE) ,
                      mBytesReceived(0) ,
                      mBodyBytesReceived(0) ,
                      mCompressHeaderChecked(PR_FALSE) ,
                      mChunkHeaderChecked(PR_FALSE) ,
                      mDataReceived(PR_FALSE) ,
                      mPipelinedRequest(request) ,
                      mDoingProxySSLConnect(aDoingProxySSLConnect) 
{
    mChannel->mHTTPServerListener = this;

    nsRepository::CreateInstance(kSupportsVoidCID, NULL, 
                kSupportsVoidIID, getter_AddRefs(mChunkHeaderEOF)) ;

    if (mChunkHeaderEOF) 
        mChunkHeaderEOF->SetData(&mChunkHeaderCtx) ;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("Creating nsHTTPServerListener [this=%x].\n", this)) ;
}

nsHTTPServerListener::~nsHTTPServerListener() 
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("Deleting nsHTTPServerListener [this=%x].\n", this)) ;

    // These two should go away in the OnStopRequest() callback.
    // But, just in case...
    NS_IF_RELEASE(mResponse) ;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsHTTPServerListener::OnDataAvailable(nsIChannel* channel,
                                      nsISupports* context,
                                      nsIInputStream *i_pStream, 
                                      PRUint32 i_SourceOffset,
                                      PRUint32 i_Length) 
{
    nsresult rv = NS_OK, channelStatus = NS_OK;
    PRUint32 actualBytesRead;

    if (mChannel) 
        mChannel->GetStatus(&channelStatus) ;

    if (NS_FAILED(channelStatus)) // Canceled http channel
         return NS_OK;

    NS_ASSERTION(i_pStream, "No stream supplied by the transport!") ;
    nsCOMPtr<nsIInputStream> bufferInStream = 
        do_QueryInterface(i_pStream) ;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("nsHTTPServerListener::OnDataAvailable [this=%x].\n"
            "\tstream=%x. \toffset=%d. \tlength=%d.\n",
            this, i_pStream, i_SourceOffset, i_Length)) ;

    if (i_Length > 0) 
        mDataReceived = PR_TRUE;

    while (!mHeadersDone) 
    {
        if (i_Length == 0) 
            return NS_OK;

        if (!mResponse) 
        {
            mResponse = new nsHTTPResponse() ;
            if (!mResponse) 
            {
                NS_ERROR("Failed to create the response object!") ;
                return NS_ERROR_OUT_OF_MEMORY;
            }
            NS_ADDREF(mResponse) ;
            mChannel->SetResponse(mResponse) ;
        }
    
        //
        // Parse the status line and the response headers from the server
        //

        //
        // Parse the status line from the server.  This is always the 
        // first line of the response...
        //
        if (!mFirstLineParsed) 
        {
            rv = ParseStatusLine(bufferInStream, i_Length, &actualBytesRead) ;
            NS_ASSERTION(i_Length - actualBytesRead <= i_Length, "wrap around") ;
            i_Length -= actualBytesRead;
        }

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("\tOnDataAvailable [this=%x]. Parsing Headers\n", this)) ;
        //
        // Parse the response headers as long as there is more data and
        // the headers are not done...
        //
        while (NS_SUCCEEDED(rv) && i_Length && !mHeadersDone) 
        {
            rv = ParseHTTPHeader(bufferInStream, i_Length, &actualBytesRead) ;
            NS_ASSERTION(i_Length - actualBytesRead <= i_Length, "wrap around") ;
            i_Length -= actualBytesRead;
        }

        if (NS_FAILED(rv)) return rv;
        
        // Don't do anything else until all headers have been parsed
        if (!mHeadersDone) 
            return NS_OK;

        //
        // All the headers have been read.
        //

        if (mResponse) 
        {
            PRUint32 statusCode = 0;
            mResponse->GetStatus(&statusCode) ;

            if (statusCode == 304) // no content
            {
                rv = FinishedResponseHeaders() ;
                if (NS_FAILED(rv)) 
                    return rv;

                rv = mPipelinedRequest->AdvanceToNextRequest() ;
                if (NS_FAILED(rv)) 
                {
                    mHandler->ReleasePipelinedRequest(mPipelinedRequest) ;
                    mPipelinedRequest = nsnull;

                    nsCOMPtr<nsISocketTransport> trans = do_QueryInterface(channel, &rv) ;

                    // XXX/ruslan: will be replaced with the new Cancel(code) 
                    if (NS_SUCCEEDED(rv)) 
                        trans->SetBytesExpected(0) ;
               }
               else
               {
                   OnStartRequest(nsnull, nsnull) ;
               }
            }
            else
            if (statusCode == 100) // Continue
            {
                mHeadersDone     = PR_FALSE;
                mFirstLineParsed = PR_FALSE;
                mHeaderBuffer.Truncate() ;

                mChannel->SetResponse(nsnull) ;
                NS_RELEASE(mResponse) ;

                mResponse = nsnull;
                mBytesReceived = 0;

                PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
                    ("\tOnDataAvailable [this=%x].(100) Continue\n", this)) ;
            }
            else
            if (statusCode == 200 && mDoingProxySSLConnect) 
            {
                mDoingProxySSLConnect = PR_FALSE;

                mHeadersDone     = PR_FALSE;
                mFirstLineParsed = PR_FALSE;
                mHeaderBuffer.Truncate() ;

                mChannel->SetResponse(nsnull) ;
                NS_RELEASE(mResponse) ;

                mResponse = nsnull;
                mBytesReceived = 0;
                mPipelinedRequest->RestartRequest(REQUEST_RESTART_SSL) ;

                return NS_OK;

                PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
                        ("\tOnDataAvailable [this=%x].(200) SSL CONNECT\n", 
                        this)) ;
            }
            else
            {
                rv = FinishedResponseHeaders() ;
                if (NS_FAILED(rv)) 
                    return rv;
            }
        } /* mResponse */
    } /* while (!mHeadersDone) */

    // At this point we've digested headers from the server and we're
    // onto the actual data. If this transaction was initiated without
    // an AsyncRead, we just want to pass the OnData() notifications
    // straight through to the consumer.
    //

    if (mSimpleResponse && mHeaderBuffer.Length()) 
    {

        const char * cp = mHeaderBuffer.GetBuffer() ;
        nsCOMPtr<nsIByteArrayInputStream>   is;

        nsresult rv1 = 
            NS_NewByteArrayInputStream(getter_AddRefs(is) , strdup(cp) , mHeaderBuffer.Length()) ;            
        
        if (NS_SUCCEEDED(rv1)) 
            mResponseDataListener->OnDataAvailable(mChannel, 
                    mChannel->mResponseContext, is, 0, mHeaderBuffer.Length()) ;
        
        mSimpleResponse = PR_FALSE;
    }

    //
    // Abort the connection if the consumer has been released.  This will 
    // happen if a redirect has been processed...
    //
    if (!mResponseDataListener) {
        // XXX: What should the return code be?
        rv = NS_BINDING_ABORTED;
    }

    if (NS_SUCCEEDED(rv) && i_Length) {
        PRBool bApplyConversion = PR_TRUE;
        (void) (mChannel->GetApplyConversion(&bApplyConversion));

        if (bApplyConversion && !mCompressHeaderChecked) 
        {
            nsXPIDLCString compressHeader;
            rv = mResponse->GetHeader(nsHTTPAtoms::Content_Encoding, 
                    getter_Copies(compressHeader)) ;
            mCompressHeaderChecked = PR_TRUE;

            if (NS_SUCCEEDED(rv) && compressHeader) 
            {
                NS_WITH_SERVICE(nsIStreamConverterService, 
                        StreamConvService, kStreamConverterServiceCID, &rv) ;
                if (NS_FAILED(rv)) return rv;

                nsString fromStr; fromStr.AssignWithConversion(compressHeader) ;
                nsString toStr;     toStr.AssignWithConversion("uncompressed") ;

                nsCOMPtr<nsIStreamListener> converterListener;
                rv = StreamConvService->AsyncConvertData(
                        fromStr.GetUnicode() , 
                        toStr.GetUnicode() , 
                        mResponseDataListener, 
                        channel, 
                        getter_AddRefs(converterListener)) ;
                if (NS_FAILED(rv)) return rv;
                mResponseDataListener = converterListener;
            }
        }

        if (!mChunkHeaderChecked) 
        {
            mChunkHeaderChecked = PR_TRUE;

            nsXPIDLCString chunkHeader;
            rv = mResponse->GetHeader(nsHTTPAtoms::Transfer_Encoding, getter_Copies(chunkHeader)) ;

            nsXPIDLCString trailerHeader;
            rv = mResponse->GetHeader(nsHTTPAtoms::Trailer, getter_Copies(trailerHeader)) ;

            if (NS_SUCCEEDED(rv) && chunkHeader) 
            {
                NS_WITH_SERVICE(nsIStreamConverterService, 
                        StreamConvService, kStreamConverterServiceCID, &rv) ;
                if (NS_FAILED(rv)) return rv;

                nsString fromStr; fromStr.AssignWithConversion( "chunked" ) ;
                nsString toStr;     toStr.AssignWithConversion("unchunked") ;

                mChunkHeaderCtx.SetEOF(PR_FALSE) ;
                if (trailerHeader) 
                {
                    nsCString ts(trailerHeader) ;
                    ts.StripWhitespace() ;

                    char *cp = ts;

                    while (*cp) 
                    {
                        char * pp = PL_strchr(cp , ',') ;
                        if (pp == NULL) 
                        {
                            mChunkHeaderCtx.AddTrailerHeader(cp) ;
                            break;
                        }
                        else
                        {
                            *pp = 0;
                            mChunkHeaderCtx.AddTrailerHeader(cp) ;
                            *pp = ',';
                            cp = pp + 1;
                        }
                    }
                }

                nsCOMPtr<nsIStreamListener> converterListener;
                rv = StreamConvService->AsyncConvertData(
                        fromStr.GetUnicode() , 
                        toStr.GetUnicode() , 
                        mResponseDataListener, 
                        mChunkHeaderEOF,
                        getter_AddRefs(converterListener)) ;
                if (NS_FAILED(rv)) return rv;
                mResponseDataListener = converterListener;
            }
        }

        rv = mResponseDataListener->OnDataAvailable(mChannel, mChannel->mResponseContext, i_pStream, 0, i_Length) ;

        PRInt32 cl = -1;
        mResponse->GetContentLength(&cl) ;

        mBodyBytesReceived += i_Length;

        // Report progress
        rv = mChannel->ReportProgress(mBodyBytesReceived, cl) ;
        if (NS_FAILED(rv)) return rv;

        PRBool eof = mChunkHeaderCtx.GetEOF() ;

        if (mPipelinedRequest
                &&(cl != -1 && cl - mBodyBytesReceived == 0 || eof)) 
        {
            if (eof && mResponse) 
            {
                nsVoidArray *mh = mChunkHeaderCtx.GetAllHeaders() ;

                for (int i = mh->Count() - 1; i >= 0; i--) 
                {
                    nsHTTPChunkConvHeaderEntry *he =(nsHTTPChunkConvHeaderEntry *) mh->ElementAt(i) ;
                    if (he) 
                    {
                        nsCOMPtr<nsIAtom> hAtom = dont_AddRef(NS_NewAtom(he->mName)) ;
                        mResponse->SetHeader(hAtom, he->mValue) ;
                    }
                }
            }
            nsresult rv1 = mPipelinedRequest->AdvanceToNextRequest() ;

            if (NS_FAILED(rv1)) 
            {
                mHandler->ReleasePipelinedRequest(mPipelinedRequest) ;
                mPipelinedRequest = nsnull;

                nsCOMPtr<nsISocketTransport> trans = do_QueryInterface(channel, &rv1) ;

                // XXX/ruslan: will be replaced with the new Cancel(code) 
                if (NS_SUCCEEDED(rv1)) 
                    trans->SetBytesExpected(0) ;

            }
            else
            {
                PRUint32 status = 0;
                if (mResponse) 
                    mResponse->GetStatus(&status) ;

                if (status != 304 || !mChannel->mCachedResponse) 
                {
                    mChannel->ResponseCompleted(mResponseDataListener, NS_OK, nsnull) ;
                    mChannel->mHTTPServerListener = 0;
                }

                OnStartRequest(nsnull, nsnull) ;

                PRUint32 streamLen = 0;
                i_pStream->Available(&streamLen) ;

                if (streamLen > 0) 
                    OnDataAvailable(channel, context, i_pStream, 0, streamLen) ;
            }
        }
    }
    return rv;
}


NS_IMETHODIMP
nsHTTPServerListener::OnStartRequest(nsIChannel* channel, nsISupports* i_pContext) 
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("nsHTTPServerListener::OnStartRequest [this=%x].\n", this)) ;

    // Initialize header varaibles...  
    mHeadersDone     = PR_FALSE;
    mFirstLineParsed = PR_FALSE;
    mCompressHeaderChecked = PR_FALSE;
    mChunkHeaderChecked    = PR_FALSE;
    mDataReceived          = PR_FALSE;
    mBytesReceived     = 0;
    mBodyBytesReceived = 0;
    mHeaderBuffer.Truncate() ;

    NS_IF_RELEASE(mResponse) ;
    NS_IF_RELEASE(mChannel) ;

    mResponse = nsnull;
    mChannel  = nsnull;
    mResponseDataListener = null_nsCOMPtr() ;

    mChunkHeaderCtx.SetEOF(PR_FALSE) ;

    nsHTTPRequest * req;
    mPipelinedRequest->GetCurrentRequest(&req) ;
    
    if (req) 
    {
        mChannel = req->mConnection;
        if (mChannel) 
        {
            mChannel->mHTTPServerListener = this;
            NS_ADDREF(mChannel) ;
        }
        NS_RELEASE(req) ;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsHTTPServerListener::OnStopRequest(nsIChannel* channel, nsISupports* i_pContext, 
                                    nsresult i_Status, const PRUnichar* aStatusArg) 
{
    nsresult rv = i_Status, channelStatus = NS_OK;

    if (mChannel) 
        mChannel->GetStatus(&channelStatus) ;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPServerListener::OnStopRequest [this=%x]."
            "\tStatus = %x, mDataReceived=%d\n", this, i_Status, mDataReceived)) ;

    if (NS_SUCCEEDED(channelStatus) && !mDataReceived
        &&(NS_SUCCEEDED(i_Status) || i_Status == NS_ERROR_FAILURE)) // EOF or not a well-known error, like timeout
    {
        // no data has been received from the channel at all - must be due to the fact that the
        // server has dropped the connection on keep-alive

        if (mPipelinedRequest) 
        {
            rv = mPipelinedRequest->RestartRequest(REQUEST_RESTART_NORMAL) ;
            if (NS_SUCCEEDED(rv)) 
            {
//                NS_IF_RELEASE(mChannel) ;
                return rv;
            }
        }
    }

    if (NS_SUCCEEDED(i_Status) && !mHeadersDone) 
    {
        //
        // Oh great!!  The server has closed the connection without sending 
        // an entity.  Assume that it has sent all the response headers and
        // process them - in case the status indicates that some action should
        // be taken(ie. redirect) .
        //
        // Ignore the return code, since the request is being completed...
        //

        mHeaderBuffer.CompressSet(" \t", ' ');
        mHeaderBuffer.StripChars ("\r\n");

        mResponse->ParseHeader (mHeaderBuffer);

        mHeadersDone = PR_TRUE;
        if (mResponse) {
            FinishedResponseHeaders() ;
        }
    }

    // Notify the HTTPChannel that the response has completed...
    NS_ASSERTION(mChannel, "HTTPChannel is null.") ;
    if (mChannel) 
    {
        PRUint32 status = 0;
        if (mResponse) 
            mResponse->GetStatus(&status) ;

        if (status != 304 || !mChannel->mCachedResponse) 
        {
            mChannel->ResponseCompleted(mResponseDataListener, i_Status, aStatusArg);
            mChannel->mHTTPServerListener = 0;
        }

        PRUint32 capabilities = 0;

        PRUint32 keepAliveTimeout = 0;
        PRInt32  keepAliveMaxCon = -1;

        if (mResponse && channel) // this is the actual response from the transport
        {
            HTTPVersion ver;
            rv = mResponse->GetServerVersion(&ver) ;
            if (NS_SUCCEEDED(rv)) 
            {
                nsXPIDLCString connectionHeader;
                PRBool usingProxy = PR_FALSE;
                
                if (mChannel) 
                    mChannel->GetUsingProxy(&usingProxy) ;

                if (usingProxy) 
                    rv = mResponse->GetHeader(nsHTTPAtoms::Proxy_Connection, getter_Copies(connectionHeader)) ;
                else
                    rv = mResponse->GetHeader(nsHTTPAtoms::Connection      , getter_Copies(connectionHeader)) ;

                if (ver == HTTP_ONE_ONE) 
                {
                    // ruslan: some older incorrect 1.1 servers may do this
                    if (NS_SUCCEEDED(rv) && connectionHeader && !PL_strcasecmp(connectionHeader,    "close")) 
                        capabilities = nsIHTTPProtocolHandler::DONTRECORD_CAPABILITIES;
                    else
                    {
                        capabilities =(usingProxy ? nsIHTTPProtocolHandler::ALLOW_PROXY_KEEPALIVE|nsIHTTPProtocolHandler::ALLOW_PROXY_PIPELINING : nsIHTTPProtocolHandler::ALLOW_KEEPALIVE|nsIHTTPProtocolHandler::ALLOW_PIPELINING) ;

                        nsXPIDLCString serverHeader;
                        rv = mResponse->GetHeader(nsHTTPAtoms::Server, getter_Copies(serverHeader)) ;

                        if (NS_SUCCEEDED(rv)) 
                            mHandler->Check4BrokenHTTPServers(serverHeader, &capabilities) ;
                    }
                }
                else
                if (ver == HTTP_ONE_ZERO) 
                {
                    if (NS_SUCCEEDED(rv) && connectionHeader && !PL_strcasecmp(connectionHeader, "keep-alive")) 
                        capabilities =(usingProxy ? NS_STATIC_CAST(unsigned long, nsIHTTPProtocolHandler::ALLOW_PROXY_KEEPALIVE) : NS_STATIC_CAST(unsigned long, nsIHTTPProtocolHandler::ALLOW_KEEPALIVE)) ;
                }

                nsXPIDLCString keepAliveHeader;
                rv = mResponse->GetHeader(nsHTTPAtoms::Keep_Alive, getter_Copies(keepAliveHeader)) ;

                const char *
                cp = PL_strstr(keepAliveHeader, "max=") ;

                if (cp) 
                    keepAliveMaxCon = atoi(cp + 4) ;

                cp = PL_strstr(keepAliveHeader, "timeout=") ;
                if (cp) 
                    keepAliveTimeout =(PRUint32) atoi(cp + 8) ;
            }
        }

        if (mPipelinedRequest) 
        {
            while (NS_SUCCEEDED(mPipelinedRequest->AdvanceToNextRequest()) ) 
            {
                OnStartRequest(nsnull, nsnull) ;
                mChannel->ResponseCompleted(mResponseDataListener, i_Status, aStatusArg);
                mChannel->mHTTPServerListener = 0;
            }

            mHandler->ReleasePipelinedRequest(mPipelinedRequest) ;
            mPipelinedRequest = nsnull;
        }

        if (channel) 
            mHandler->ReleaseTransport(channel, capabilities, PR_FALSE, keepAliveTimeout, keepAliveMaxCon) ;
    }

    NS_IF_RELEASE(mChannel) ;
    NS_IF_RELEASE(mResponse) ;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTTPServerListener methods:

nsresult nsHTTPServerListener::Abort() 
{
  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("nsHTTPServerListener::Abort [this=%x].", this)) ;

  //
  // Clearing the data consumer will cause the response to abort.  This
  // also prevents any more notifications from being passed out to the consumer.
  //
  mResponseDataListener = 0;

  return NS_OK;
}


nsresult nsHTTPServerListener::FireSingleOnData(nsIStreamListener *aListener, 
        nsISupports *aContext) 
{
    nsresult rv;

    if (mHeadersDone) {
        rv = FinishedResponseHeaders() ;
        if (NS_FAILED(rv)) return rv;
        
        if (mBytesReceived && mResponseDataListener) {
            rv = mResponseDataListener->OnDataAvailable(mChannel, 
                    mChannel->mResponseContext,
                    mDataStream, 0, mBytesReceived) ;
        }
        mDataStream = 0;
    }
    
    return rv;
}

static NS_METHOD
nsWriteLineToString(nsIInputStream* in,
                    void* closure,
                    const char* fromRawSegment,
                    PRUint32 offset,
                    PRUint32 count,
                    PRUint32 *writeCount) 
{
  nsCString* str = (nsCString*)closure;
  *writeCount = 0;

  const char* buf = fromRawSegment;
  while (count-- > 0) {
    const char c = *buf++;
    (*writeCount)++;
    if (c == LF) {
      break;
    }
  }
  str->Append(fromRawSegment, *writeCount);
  return NS_BASE_STREAM_WOULD_BLOCK;
}


nsresult nsHTTPServerListener::ParseStatusLine(nsIInputStream* in, 
                                               PRUint32 aLength,
                                               PRUint32 *aBytesRead) 
{
  nsresult rv = NS_OK;
  PRUint32 actualBytesRead;

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPServerListener::ParseStatusLine [this=%x].\taLength=%d\n", 
          this, aLength)) ;

  *aBytesRead = 0;

  if (kMAX_HEADER_SIZE < mHeaderBuffer.Length()) {
    // This server is yanking our chain...
    return NS_ERROR_FAILURE;
  }

  rv = in->ReadSegments(nsWriteLineToString, 
                        (void*) &mHeaderBuffer, 
                        aLength, 
                        &actualBytesRead) ;
  if (NS_FAILED(rv)) return rv;

  *aBytesRead += actualBytesRead;

  PRUint32 bL = mHeaderBuffer.Length() ;

  if (bL > 0 && mHeaderBuffer.Find("HTTP/", PR_FALSE, 0, 5) != 0) 
  {
      // this is simple http response
      mSimpleResponse = PR_TRUE;
      mHeadersDone    = PR_TRUE;
      mFirstLineParsed= PR_TRUE;

      mResponse->SetStatus(200) ;
      mResponse->SetServerVersion("HTTP/1.0") ;
      mResponse->SetContentType("text/html") ;

      return NS_OK;
  }

  // Wait for more data to arrive before processing the header...
  if (bL > 0 && mHeaderBuffer.CharAt(bL - 1) != LF) return NS_OK;

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("\tParseStatusLine [this=%x].\tGot Status-Line:%s\n"
         , this, mHeaderBuffer.GetBuffer()) ) ;

  //
  // Replace all LWS with single SP characters.  Also remove the CRLF
  // characters...
  //
  mHeaderBuffer.CompressSet(" \t", ' ') ;
  mHeaderBuffer.Trim("\r\n", PR_FALSE) ;

  rv = mResponse->ParseStatusLine(mHeaderBuffer) ;
  if (NS_SUCCEEDED(rv)) {
    HTTPVersion ver;

    rv = mResponse->GetServerVersion(&ver) ;
    if (HTTP_ZERO_NINE == ver) {
      //
      // This is a HTTP/0.9 response...
      // Pretend that the headers have been consumed.
      //
      PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
             ("\tParseStatusLine [this=%x]. HTTP/0.9 Server Response!"
              " Hold onto you seats!\n", this)) ;

      mResponse->SetStatus(200) ;

      // XXX: There will be no Content-Type or Content-Length headers!
      mHeadersDone = PR_TRUE;
    }
  }

  mFirstLineParsed = PR_TRUE;

  return rv;
}

nsresult nsHTTPServerListener::ParseHTTPHeader(nsIInputStream* in,
                                               PRUint32 aLength,
                                               PRUint32 *aBytesRead) 
{
  nsresult rv = NS_OK;
  PRUint32 totalBytesToRead = aLength, actualBytesRead;

  *aBytesRead = 0;

  if (kMAX_HEADER_SIZE < mHeaderBuffer.Length()) {
    // This server is yanking our chain...
    return NS_ERROR_FAILURE;
  }

  //
  // Read the header from the input buffer...  A header is terminated by 
  // a CRLF.  Header values may be extended over multiple lines by preceeding
  // each extra line with linear white space...
  //
  PRInt32 newlineOffset = 0;
  do {
    // Append the buffer into the header string...
    rv = in->ReadSegments(nsWriteLineToString, 
                          (void*) &mHeaderBuffer, 
                          totalBytesToRead, 
                          &actualBytesRead);
    if (NS_FAILED(rv)) return rv;
    if (actualBytesRead == 0)
      break;

    *aBytesRead += actualBytesRead;
    totalBytesToRead -= actualBytesRead;

    //
    // If last character in the header string is a LF, then the header 
    // may be complete...
    //
    newlineOffset = mHeaderBuffer.FindChar(LF, PR_FALSE, newlineOffset);
    if (newlineOffset == -1)
      return NS_OK;
    
    // This line is either LF or CRLF so the header is complete...
    if (mHeaderBuffer.Length() <= 2) {
      break;
    }

    if ((PRUint32)newlineOffset == mHeaderBuffer.Length())
      return NS_OK;

    const char* buf = mHeaderBuffer.GetBuffer();
    const char charAfterNewline = buf[newlineOffset + 1];
    if ((charAfterNewline != ' ') || (charAfterNewline != '\t'))
      break;
    newlineOffset++;
  } while (PR_TRUE);

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("\tParseHTTPHeader [this=%x].\tGot header string:%s\n",
          this, mHeaderBuffer.GetBuffer()) ) ;

  //
  // Replace all LWS with single SP characters.  And remove all of the CRLF
  // characters...
  //
  mHeaderBuffer.CompressSet(" \t", ' ') ;
  mHeaderBuffer.StripChars("\r\n") ;

  if (mHeaderBuffer.Length() == 0) {
    mHeadersDone = PR_TRUE;
    return NS_OK;
  }

  return mResponse->ParseHeader(mHeaderBuffer) ;
}

nsresult
nsHTTPServerListener::FinishedResponseHeaders() 
{
    nsresult rv;

    rv = mChannel->FinishedResponseHeaders() ;
    if (NS_FAILED(rv)) return rv;

    //
    // Fire the OnStartRequest notification - now that user data is available
    //
    if (NS_SUCCEEDED(rv) && mResponseDataListener) 
    {
        rv = mResponseDataListener->OnStartRequest(mChannel, mChannel->mResponseContext) ;
        if (NS_FAILED(rv)) 
        {
            PR_LOG(gHTTPLog, PR_LOG_ERROR,("\tOnStartRequest [this=%x]. Consumer failed!"
                        "Status: %x\n", this, rv)) ;
        }
    } 

    return rv;
}

// -----------------------------------------------------------------------------
// nsHTTPFinalListener

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_THREADSAFE_ADDREF(nsHTTPFinalListener) 
NS_IMPL_THREADSAFE_RELEASE(nsHTTPFinalListener) 

NS_IMPL_QUERY_INTERFACE2(nsHTTPFinalListener, 
                           nsIStreamListener, 
                           nsIStreamObserver) ;

static  PRUint32    sFinalListenersCreated = 0;
static  PRUint32    sFinalListenersDeleted = 0;

nsHTTPFinalListener::nsHTTPFinalListener(
        nsHTTPChannel* aChannel, nsIStreamListener* aListener, nsISupports *aContext) 
    :
    mOnStartFired(PR_FALSE) ,
    mOnStopFired(PR_FALSE) ,
    mShutdown(PR_FALSE) ,
    mBusy(PR_FALSE) ,
    mOnStopPending(PR_FALSE) 
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("Creating nsHTTPFinalListener [this=%x], created=%u, deleted=%u\n", 
            this, ++sFinalListenersCreated, sFinalListenersDeleted)) ;

    NS_INIT_REFCNT() ;

    mChannel  =  aChannel;
    mContext  =  aContext;
    mListener = aListener;

    NS_ASSERTION(aChannel, "HTTPChannel is null.") ;
    NS_ADDREF(mChannel) ;
}

nsHTTPFinalListener::~nsHTTPFinalListener() 
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
("Deleting nsHTTPFinalListener [this=%x], created=%u, deleted=%u\n",
            this, sFinalListenersCreated, ++sFinalListenersDeleted)) ;

    NS_IF_RELEASE(mChannel) ;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPFinalListener::OnStartRequest(nsIChannel *aChannel,
                                    nsISupports *aContext) 
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,
("nsHTTPFinalListener::OnStartRequest [this=%x]"
            ", mOnStartFired=%u\n", this, mOnStartFired)) ;

    if (mShutdown || mOnStartFired || mBusy) 
        return NS_OK;

    mOnStartFired = PR_TRUE;
    return mListener->OnStartRequest(aChannel, aContext) ;
}

NS_IMETHODIMP
nsHTTPFinalListener::OnStopRequest(nsIChannel *aChannel, nsISupports *aContext,
                                   nsresult aStatus, const PRUnichar* aStatusArg) 
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,
           ("nsHTTPFinalListener::OnStopRequest [this=%x]"
            ", mOnStopFired=%u\n", this, mOnStopFired)) ;

    if (mShutdown || mOnStopFired) 
        return NS_OK;

    if (mBusy) 
    {
        mOnStopPending = PR_TRUE;
        return NS_OK;
    }

    if (mChannel) 
    {
        PRUint32 status;
        mChannel->GetStatus(&status) ;

        if (NS_FAILED(status) && NS_SUCCEEDED(aStatus)) 
            aStatus = status;
    }

    if (!mOnStartFired) 
    {
        // XXX/ruslan: uncomment me when the webshell is fixed
        // mOnStartFired = PR_TRUE;
        // mListener->OnStartRequest(aChannel, aContext) ;
    }

    mOnStopFired = PR_TRUE;
    nsresult rv = mListener->OnStopRequest(aChannel, aContext, aStatus, aStatusArg) ;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsHTTPFinalListener::OnDataAvailable(nsIChannel *aChannel,
                                     nsISupports *aContext,
                                     nsIInputStream *aStream,
                                     PRUint32 aSourceOffset,
                                     PRUint32 aCount) 
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,
("nsHTTPFinalListener::OnDataAvailable [this=%x]\n",
            this)) ;

    if (!mShutdown) 
    {
        PRUint32 status;
        mChannel->GetStatus(&status) ;

        if (NS_SUCCEEDED(status)) 
        {
            NS_ASSERTION(mOnStopFired == PR_FALSE, "OnDataAvailable fired after OnStopRequest") ;

            if (mOnStopFired) 
                return NS_OK;

            mBusy = PR_TRUE;
            nsresult rv = mListener->OnDataAvailable(aChannel, aContext, 
                                aStream, aSourceOffset, aCount) ;
            
            mBusy = PR_FALSE;

            if (mOnStopPending) 
                OnStopRequest(mChannel, mContext, NS_OK, nsnull) ;

            return rv;
        }
    }

    return NS_OK;
}

void
nsHTTPFinalListener::FireNotifications() 
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG,
("nsHTTPFinalListener::FireNotifications [this=%x]\n",
            this)) ;

    if (!mShutdown) 
        OnStopRequest(mChannel, mContext, NS_OK, nsnull) ;
}

void
nsHTTPFinalListener::Shutdown() 
{
    mShutdown = PR_TRUE;
}

// -----------------------------------------------------------------------------
