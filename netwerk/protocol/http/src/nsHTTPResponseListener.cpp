/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIBufferInputStream.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponse.h"
#include "nsCRT.h"
#include "nsIStreamConverterService.h"
#include "nsIStreamConverter.h"

#include "nsHTTPAtoms.h"
#include "nsIHttpNotify.h"
#include "nsINetModRegEntry.h"
#include "nsProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsINetModuleMgr.h"
#include "nsIEventQueueService.h"

#include "nsXPIDLString.h" 

#include "nsIIOService.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

//
// This specifies the maximum allowable size for a server Status-Line
// or Response-Header.
//
static const int kMAX_HEADER_SIZE = 60000;

static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);

nsHTTPResponseListener::nsHTTPResponseListener(nsHTTPChannel *aChannel, nsHTTPHandler *handler)
                       : mChannel(aChannel), mHandler (handler)
{
  NS_INIT_REFCNT();

  // mChannel is not an interface pointer, so COMPtrs cannot be used :-(
  NS_ASSERTION(aChannel, "HTTPChannel is null.");
  NS_ADDREF(mChannel);

#if defined(PR_LOGGING)
  nsCOMPtr<nsIURI> url;
  nsXPIDLCString urlCString; 

  aChannel->GetURI(getter_AddRefs(url));
  if (url) {
    url->GetSpec(getter_Copies(urlCString));
  }
  
  PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
         ("Creating nsHTTPResponseListener [this=%x] for URI: %s.\n", 
           this, (const char *)urlCString));
#endif
}

nsHTTPResponseListener::~nsHTTPResponseListener()
{
  // mChannel is not an interface pointer, so COMPtrs cannot be used :-(
  NS_IF_RELEASE(mChannel);
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
                         nsIStreamObserver);



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
         ("Creating nsHTTPCacheListener [this=%x].\n", this));

}

nsHTTPCacheListener::~nsHTTPCacheListener()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPCacheListener [this=%x].\n", this));
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPCacheListener::OnStartRequest(nsIChannel *aChannel,
                                    nsISupports *aContext)
{
  PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
         ("nsHTTPCacheListener::OnStartRequest [this=%x].\n",
          this));

  return mResponseDataListener->OnStartRequest(mChannel, aContext);
}

NS_IMETHODIMP
nsHTTPCacheListener::OnStopRequest(nsIChannel *aChannel,
                                   nsISupports *aContext,
                                   nsresult aStatus,
                                   const PRUnichar *aErrorMsg)
{
  PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
         ("nsHTTPCacheListener::OnStopRequest [this=%x]."
          "\tStatus = %x\n", this, aStatus));

  //
  // Notify the channel that the response has finished.  Since there
  // is no socket transport involved nsnull is passed as the
  // transport...
  //
  return mChannel->ResponseCompleted(mResponseDataListener, 
                                     aStatus, 
                                     aErrorMsg);
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
  return mResponseDataListener->OnDataAvailable(mChannel, 
                                                aContext,
                                                aStream,
                                                aSourceOffset,
                                                aCount);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTTPResponseListener methods:

nsresult
nsHTTPCacheListener::FireSingleOnData(nsIStreamListener *aListener, 
                                      nsISupports *aContext)
{
  NS_ASSERTION(0, "nsHTTPCacheListener::FireSingleOnData(...) "
                  "should never be called.");
 
  return NS_ERROR_FAILURE;
}

nsresult nsHTTPCacheListener::Abort()
{
  NS_ASSERTION(0, "nsHTTPCachedResponseListener::Abort() "
                  "should never be called.");
 
  return NS_ERROR_FAILURE;
}


////////////////////////////////////////////////////////////////////////////////
//
// nsHTTPServerListener Implementation
//
// This subclass of nsHTTPResponseListener processes responses from
// HTTP servers.
//
////////////////////////////////////////////////////////////////////////////////

nsHTTPServerListener::nsHTTPServerListener(nsHTTPChannel* aChannel, nsHTTPHandler *handler)
                    : nsHTTPResponseListener (aChannel, handler),
                      mResponse(nsnull),
                      mFirstLineParsed(PR_FALSE),
                      mHeadersDone(PR_FALSE),
                      mBytesReceived(0),
                      mBodyBytesReceived (0),
                      mCompressHeaderChecked (PR_FALSE),
                      mChunkHeaderChecked (PR_FALSE)
{
    mChannel->mHTTPServerListener = this;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Creating nsHTTPServerListener [this=%x].\n", this));
}

nsHTTPServerListener::~nsHTTPServerListener()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPServerListener [this=%x].\n", this));

    // These two should go away in the OnStopRequest() callback.
    // But, just in case...
    NS_IF_RELEASE(mResponse);
}

static NS_DEFINE_IID(kProxyObjectManagerIID, NS_IPROXYEVENT_MANAGER_IID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsHTTPServerListener::OnDataAvailable(nsIChannel* channel,
                                      nsISupports* context,
                                      nsIInputStream *i_pStream, 
                                      PRUint32 i_SourceOffset,
                                      PRUint32 i_Length)
{
    nsresult rv = NS_OK;
    PRUint32 actualBytesRead;
    NS_ASSERTION(i_pStream, "No stream supplied by the transport!");
    nsCOMPtr<nsIBufferInputStream> bufferInStream = 
        do_QueryInterface(i_pStream);

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPServerListener::OnDataAvailable [this=%x].\n"
            "\tstream=%x. \toffset=%d. \tlength=%d.\n",
            this, i_pStream, i_SourceOffset, i_Length));

    if (!mResponse)
    {
        mResponse = new nsHTTPResponse ();
        if (!mResponse) {
            NS_ERROR("Failed to create the response object!");
            return NS_ERROR_OUT_OF_MEMORY;
        }
        NS_ADDREF(mResponse);
        mChannel->SetResponse(mResponse);
    }
    //
    // Parse the status line and the response headers from the server
    //
    if (!mHeadersDone) {
        //
        // Parse the status line from the server.  This is always the 
        // first line of the response...
        //
        if (!mFirstLineParsed) {
            rv = ParseStatusLine(bufferInStream, i_Length, &actualBytesRead);
            NS_ASSERTION(i_Length - actualBytesRead <= i_Length, "wrap around");
            i_Length -= actualBytesRead;
        }

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("\tOnDataAvailable [this=%x]. Parsing Headers\n", this));
        //
        // Parse the response headers as long as there is more data and
        // the headers are not done...
        //
        while (NS_SUCCEEDED(rv) && i_Length && !mHeadersDone) {
            rv = ParseHTTPHeader(bufferInStream, i_Length, &actualBytesRead);
            NS_ASSERTION(i_Length - actualBytesRead <= i_Length, "wrap around");
            i_Length -= actualBytesRead;
        }

        if (NS_FAILED(rv)) return rv;
        
        // Don't do anything else until all headers have been parsed
        if (!mHeadersDone)
            return NS_OK;

        //
        // All the headers have been read.
        //
        rv = FinishedResponseHeaders ();
        if (NS_FAILED(rv)) return rv;

        if (mResponse)
        {
            PRUint32 statusCode = 0;
            mResponse -> GetStatus (&statusCode);
            if (statusCode == 304)  // no content
            {
                nsCOMPtr<nsISocketTransport> trans = do_QueryInterface (channel, &rv);

                // XXX/ruslan: will be replace with the new Cancel (code)
                if (NS_SUCCEEDED (rv))
		            trans -> SetBytesExpected (0);
            }
        }
    }

    // At this point we've digested headers from the server and we're
    // onto the actual data. If this transaction was initiated without
    // an AsyncRead, we just want to pass the OnData() notifications
    // straight through to the consumer.
    //
    // .... otherwise...
    //
    // if we were AsyncOpened, we want to increment our internal byte count
    // so when we finally push the stream to the consumer via AsyncRead,
    // we're sure to pass him all the data that has queued up.

    if (mChannel->mOpenObserver && !mResponseDataListener) {
        mBytesReceived += i_Length;
        mDataStream = i_pStream;
    } else {

        //
        // Abort the connection if the consumer has been released.  This will 
        // happen if a redirect has been processed...
        //
        if (!mResponseDataListener) {
            // XXX: What should the return code be?
            rv = NS_BINDING_ABORTED;
        }

        if (NS_SUCCEEDED(rv)) {
            if (i_Length) {

                PRInt32 cl = -1;
				mResponse -> GetContentLength (&cl);

                mBodyBytesReceived += i_Length;

                if (cl != -1 && cl - mBodyBytesReceived == 0)
                {
                    nsCOMPtr<nsISocketTransport> trans = do_QueryInterface (channel, &rv);

                    // XXX/ruslan: will be replaced with the new Cancel (code)
                    if (NS_SUCCEEDED (rv))
					    trans -> SetBytesExpected (0);
				}

                if (!mChunkHeaderChecked)
                {
                    mChunkHeaderChecked = PR_TRUE;
                    
                    nsXPIDLCString chunkHeader;
                    rv = mResponse -> GetHeader (nsHTTPAtoms::Transfer_Encoding, getter_Copies (chunkHeader));
                    
                    if (NS_SUCCEEDED (rv) && chunkHeader)
                    {
    					NS_WITH_SERVICE (nsIStreamConverterService, 
                                StreamConvService, kStreamConverterServiceCID, &rv);
		    			if (NS_FAILED(rv)) return rv;

			    		nsString2 fromStr ( chunkHeader);
				    	nsString2 toStr   ("unchunked" );
				    
                        nsCOMPtr<nsIStreamListener> converterListener;
					    rv = StreamConvService->AsyncConvertData(
                                fromStr.GetUnicode(), 
                                toStr.GetUnicode(), 
                                mResponseDataListener, 
                                channel, 
                                getter_AddRefs (converterListener));
					    if (NS_FAILED(rv)) return rv;
					    mResponseDataListener = converterListener;
                    }
				}

                if (!mCompressHeaderChecked)
                {
                    nsXPIDLCString compressHeader;
                    rv = mResponse -> GetHeader (nsHTTPAtoms::Content_Encoding, getter_Copies (compressHeader));
                    mCompressHeaderChecked = PR_TRUE;

                    if (NS_SUCCEEDED (rv) && compressHeader)
                    {
    					NS_WITH_SERVICE (nsIStreamConverterService, 
                                StreamConvService, kStreamConverterServiceCID, &rv);
					    if (NS_FAILED(rv)) return rv;

					    nsString2 fromStr ( compressHeader );
					    nsString2 toStr   ( "uncompressed" );
				    
                        nsCOMPtr<nsIStreamListener> converterListener;
					    rv = StreamConvService->AsyncConvertData(
                                fromStr.GetUnicode(), 
                                toStr.GetUnicode(), 
                                mResponseDataListener, 
                                channel, 
                                getter_AddRefs (converterListener));
					    if (NS_FAILED(rv)) return rv;
					    mResponseDataListener = converterListener;
                    }
                }


                PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
                       ("\tOnDataAvailable [this=%x]. Calling consumer "
                        "OnDataAvailable.\tlength:%d\n", this, i_Length));

                rv = mResponseDataListener->OnDataAvailable(mChannel, 
                        mChannel->mResponseContext,
                        i_pStream, 0, i_Length);
                if (NS_FAILED(rv)) {
                  PR_LOG(gHTTPLog, PR_LOG_ERROR, 
                         ("\tOnDataAvailable [this=%x]. Consumer failed!"
                          "Status: %x\n", this, rv));
                }
            }
        } 
    } // end !mChannel->mOpenObserver

    return rv;
}


NS_IMETHODIMP
nsHTTPServerListener::OnStartRequest(nsIChannel* channel, 
        nsISupports* i_pContext)
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPServerListener::OnStartRequest [this=%x].\n", this));

    // Initialize header varaibles...  
    mHeadersDone     = PR_FALSE;
    mFirstLineParsed = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
nsHTTPServerListener::OnStopRequest(nsIChannel* channel,
                                    nsISupports* i_pContext,
                                    nsresult i_Status,
                                    const PRUnichar* i_pMsg)
{
    nsresult rv = NS_OK;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPServerListener::OnStopRequest [this=%x]."
            "\tStatus = %x\n", this, i_Status));

    if (NS_SUCCEEDED(rv) && !mHeadersDone) {
        //
        // Oh great!!  The server has closed the connection without sending 
        // an entity.  Assume that it has sent all the response headers and
        // process them - in case the status indicates that some action should
        // be taken (ie. redirect).
        //
        // Ignore the return code, since the request is being completed...
        //
        mHeadersDone = PR_TRUE;
        if (mResponse) {
            (void)FinishedResponseHeaders();
        }
    }

    // Notify the HTTPChannel that the response has completed...
    NS_ASSERTION(mChannel, "HTTPChannel is null.");
    if (mChannel) {
        PRUint32 status = 0;

        if (mResponse) {
            mResponse->GetStatus(&status);
        }

        if (status != 304) {
            mChannel->ResponseCompleted(mResponseDataListener, 
                                        i_Status, i_pMsg);
            // The HTTPChannel no longer needs a reference to this object.
            mChannel->mHTTPServerListener = 0;
        } else {
            PR_LOG(gHTTPLog, PR_LOG_DEBUG,
                  ("nsHTTPServerListener::OnStopRequest [this=%x]. "
                   "Discarding 304 response\n", this));
        }
        PRBool keepAlive = PR_FALSE;

        if (mResponse)
        {
            HTTPVersion ver;
            rv = mResponse -> GetServerVersion (&ver);
            if (NS_SUCCEEDED (rv))
            {
                nsXPIDLCString connectionHeader;
                PRBool usingProxy = PR_FALSE;
                
                if (mChannel)
                    mChannel -> GetUsingProxy (&usingProxy);

                if (usingProxy)
                    rv = mResponse -> GetHeader (nsHTTPAtoms::Proxy_Connection, getter_Copies (connectionHeader));
                else
                    rv = mResponse -> GetHeader (nsHTTPAtoms::Connection      , getter_Copies (connectionHeader));

                if (ver == HTTP_ONE_ONE )
                {
                    // ruslan: some older incorrect 1.1 servers may do this
                    if (NS_SUCCEEDED (rv) && connectionHeader && !PL_strcmp (connectionHeader,    "close"  ))
                        keepAlive = PR_FALSE;
                    else
                        keepAlive =  PR_TRUE;
                }
                else
                if (ver == HTTP_ONE_ZERO)
                {
                    if (NS_SUCCEEDED (rv) && connectionHeader && !PL_strcmp (connectionHeader, "keep-alive"))
                        keepAlive = PR_TRUE;
                }
            }
        }

        mHandler -> ReleaseTransport (channel, keepAlive);
    }

    NS_IF_RELEASE(mChannel);
    NS_IF_RELEASE(mResponse);

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTTPServerListener methods:

nsresult nsHTTPServerListener::Abort()
{
  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPServerListener::Abort [this=%x].", this));

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
        rv = FinishedResponseHeaders();
        if (NS_FAILED(rv)) return rv;
        
        if (mBytesReceived && mResponseDataListener) {
            rv = mResponseDataListener->OnDataAvailable(mChannel, 
                    mChannel->mResponseContext,
                    mDataStream, 0, mBytesReceived);
        }
        mDataStream = 0;
    }
    
    return rv;
}

NS_METHOD
nsWriteToString(void* closure,
                const char* fromRawSegment,
                PRUint32 offset,
                PRUint32 count,
                PRUint32 *writeCount)
{
  nsString *str = (nsString*)closure;

  str->Append(fromRawSegment, count);
  *writeCount = count;
  
  return NS_OK;
}


nsresult nsHTTPServerListener::ParseStatusLine(nsIBufferInputStream* in, 
                                               PRUint32 aLength,
                                               PRUint32 *aBytesRead)
{
  nsresult rv = NS_OK;

  PRBool bFoundString = PR_FALSE;
  PRUint32 offsetOfEnd, totalBytesToRead, actualBytesRead;

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPServerListener::ParseStatusLine [this=%x].\taLength=%d\n", 
          this, aLength));

  *aBytesRead = 0;

  if (kMAX_HEADER_SIZE < mHeaderBuffer.Length()) {
    // This server is yanking our chain...
    return NS_ERROR_FAILURE;
  }

  // Look for the LF which ends the Status-Line.
  // n.b. Search looks at all pending data not just the first aLength bytes
  rv = in->Search("\n", PR_FALSE, &bFoundString, &offsetOfEnd);
  if (NS_FAILED(rv)) return rv;
  if (bFoundString && offsetOfEnd >= aLength) bFoundString = PR_FALSE;

  if (!bFoundString) {
    //
    // This is a partial header...  Read the entire buffer and wait for
    // more data...
    //
    totalBytesToRead = aLength;
  } else {
    // Do not forget to include the LF character in the read...
    totalBytesToRead = offsetOfEnd+1;
  }

  rv = in->ReadSegments(nsWriteToString, 
                        (void*)&mHeaderBuffer, 
                        totalBytesToRead, 
                        &actualBytesRead);
  if (NS_FAILED(rv)) return rv;

  *aBytesRead += actualBytesRead;

  // Wait for more data to arrive before processing the header...
  if (!bFoundString) return NS_OK;

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("\tParseStatusLine [this=%x].\tGot Status-Line:%s\n"
         , this, mHeaderBuffer.GetBuffer()));

  //
  // Replace all LWS with single SP characters.  Also remove the CRLF
  // characters...
  //
  mHeaderBuffer.CompressSet(" \t", ' ');
  mHeaderBuffer.Trim("\r\n", PR_FALSE);

  rv = mResponse->ParseStatusLine(mHeaderBuffer);
  if (NS_SUCCEEDED(rv)) {
    HTTPVersion ver;

    rv = mResponse->GetServerVersion(&ver);
    if (HTTP_ZERO_NINE == ver) {
      //
      // This is a HTTP/0.9 response...
      // Pretend that the headers have been consumed.
      //
      PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
             ("\tParseStatusLine [this=%x]. HTTP/0.9 Server Response!"
              " Hold onto you seats!\n", this));

      mResponse->SetStatus(200);

      // XXX: There will be no Content-Type or Content-Length headers!
      mHeadersDone = PR_TRUE;
    }
  }

  mFirstLineParsed = PR_TRUE;

  return rv;
}

nsresult nsHTTPServerListener::ParseHTTPHeader(nsIBufferInputStream* in,
                                               PRUint32 aLength,
                                               PRUint32 *aBytesRead)
{
  nsresult rv = NS_OK;

  PRBool bFoundString;
  PRUint32 offsetOfEnd, totalBytesToRead, actualBytesRead;

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
  do {
    //
    // If last character in the header string is a LF, then the header 
    // may be complete...
    //
    if (mHeaderBuffer.Last() == '\n' ) {
      // This line is either LF or CRLF so the header is complete...
      if (mHeaderBuffer.Length() <= 2) {
          break;
      }

      rv = in->Search(" ", PR_FALSE, &bFoundString, &offsetOfEnd);
      if (NS_FAILED(rv)) return rv;
      if (bFoundString && offsetOfEnd >= aLength) bFoundString = PR_FALSE;

      // Need to wait for more data to see if the header is complete
      if (!bFoundString && offsetOfEnd == 0) 
          return NS_OK;     

      if (!bFoundString || offsetOfEnd != 0) {
          // then check for tab too
          rv = in->Search("\t", PR_FALSE, &bFoundString, &offsetOfEnd);
          if (NS_FAILED(rv)) return rv;
          if (bFoundString && offsetOfEnd >= aLength) bFoundString = PR_FALSE;

          NS_ASSERTION(!(!bFoundString && offsetOfEnd == 0), 
                  "should have been checked above");
          if (!bFoundString || offsetOfEnd != 0) {
              break; // neither space nor tab, so jump out of the loop
          }
      }
      // else, go around the loop again and accumulate the rest of the header...
    }

    // Look for the next LF in the buffer...
    rv = in->Search("\n", PR_FALSE, &bFoundString, &offsetOfEnd);
    if (NS_FAILED(rv)) return rv;
    if (bFoundString && offsetOfEnd >= aLength) bFoundString = PR_FALSE;


    if (!bFoundString) {
      //
      // The buffer contains a partial header.  Read the entire buffer 
      // and wait for more data...
      //
      totalBytesToRead = aLength;
    } else {
    // Do not forget to include the LF character in the read...
      totalBytesToRead = offsetOfEnd+1;
    }

    // Append the buffer into the header string...
    rv = in->ReadSegments(nsWriteToString, 
                          (void*)&mHeaderBuffer, 
                          totalBytesToRead, 
                          &actualBytesRead);
    if (NS_FAILED(rv)) return rv;

    *aBytesRead += actualBytesRead;

    // Partial header - wait for more data to arrive...
    if (!bFoundString) return NS_OK;

  } while (PR_TRUE);

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("\tParseHTTPHeader [this=%x].\tGot header string:%s\n",
          this, mHeaderBuffer.GetBuffer()));

  //
  // Replace all LWS with single SP characters.  And remove all of the CRLF
  // characters...
  //
  mHeaderBuffer.CompressSet(" \t", ' ');
  mHeaderBuffer.StripChars("\r\n");

  if (mHeaderBuffer.Length() == 0) {
    mHeadersDone = PR_TRUE;
    return NS_OK;
  }

  return mResponse->ParseHeader(mHeaderBuffer);
}

nsresult nsHTTPServerListener::FinishedResponseHeaders(void)
{
  nsresult rv;

  rv = mChannel->FinishedResponseHeaders();
  if (NS_FAILED(rv)) return rv;

  //
  // Fire the OnStartRequest notification - now that user data is available
  //
  if (NS_SUCCEEDED(rv) && mResponseDataListener) {
    rv = mResponseDataListener->OnStartRequest(mChannel, 
            mChannel->mResponseContext);
    if (NS_FAILED(rv)) {
      PR_LOG(gHTTPLog, PR_LOG_ERROR, 
             ("\tOnStartRequest [this=%x]. Consumer failed!"
              "Status: %x\n", this, rv));
    }
  } 

  return rv;
}
