/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nspr.h"
#include "nsIURL.h"
#include "nsHTTPRequest.h"
#include "nsHTTPAtoms.h"
#include "nsHTTPEnums.h"
#include "nsIPipe.h"
#include "nsIStringStream.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsHTTPChannel.h"
#include "nsHTTPResponseListener.h"
#include "nsCRT.h"
#include "nsXPIDLString.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

nsHTTPRequest::nsHTTPRequest(nsIURI* i_URL, HTTPMethod i_Method, 
    nsIChannel* i_Transport):
    mMethod(i_Method),
    mVersion(HTTP_ONE_ZERO),
    mUsingProxy(PR_FALSE)
{
    NS_INIT_REFCNT();

    mURI = do_QueryInterface(i_URL);

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Creating nsHTTPRequest [this=%x].\n", this));

    mTransport = i_Transport;

    // Send Host header by default
    if (HTTP_ZERO_NINE != mVersion)
    {
        nsXPIDLCString host;
        NS_ASSERTION(mURI, "No URI for the request!!");
        mURI->GetHost(getter_Copies(host));
        PRInt32 port = -1;
        mURI->GetPort(&port);
        if (-1 != port)
        {
            char* tempHostPort = PR_smprintf("%s:%d", (const char*)host, port);
            if (tempHostPort)
            {
                SetHeader(nsHTTPAtoms::Host, tempHostPort);
                PR_smprintf_free(tempHostPort);
                tempHostPort = 0;
            }
        }
        else
            SetHeader(nsHTTPAtoms::Host, host);
    }

    // Send */*. We're no longer chopping MIME-types for acceptance.
    // MIME based content negotiation has died.
    SetHeader(nsHTTPAtoms::Accept, "*/*");
}

nsHTTPRequest::~nsHTTPRequest()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPRequest [this=%x].\n", this));

    mTransport = null_nsCOMPtr();
/*
    if (mConnection)
        NS_RELEASE(mConnection);
*/
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_ADDREF(nsHTTPRequest);
NS_IMPL_RELEASE(nsHTTPRequest);

NS_IMETHODIMP
nsHTTPRequest::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    if (aIID.Equals(NS_GET_IID(nsIStreamObserver)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamObserver*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIRequest))) {
        *aInstancePtr = NS_STATIC_CAST(nsIRequest*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsHTTPRequest::IsPending(PRBool *result)
{
  nsresult rv = NS_OK;

  if (mTransport) {
    rv = mTransport->IsPending(result);
  } else {
    *result = PR_FALSE; 
  }

  return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Cancel(void)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mTransport) {
    rv = mTransport->Cancel();
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Suspend(void)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mTransport) {
    rv = mTransport->Suspend();
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPRequest::Resume(void)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mTransport) {
    rv = mTransport->Resume();
  }
  return rv;
}


// Finally our own methods...

nsresult nsHTTPRequest::WriteRequest(nsIChannel *aTransport, PRBool aIsProxied)
{
    nsresult rv;
    if (!mURI) {
        NS_ERROR("No URL to build request for!");
        return NS_ERROR_NULL_POINTER;
    }

    NS_ASSERTION(!mTransport, "Transport being overwritten!");
    mTransport = aTransport;

    mUsingProxy = aIsProxied;
    if (mUsingProxy) {
        // Additional headers for proxy usage
        //SetHeader(nsHTTPAtoms::Pragma, "no-cache");
        // When Keep-Alive gets ready TODO
        //SetHeader(nsHTTPAtoms::Proxy-Connection, "Keep-Alive");
    }

    //
    // Build up the request into mRequestBuffer...
    //
    nsXPIDLCString autoBuffer;
    nsAutoString   autoString;

    mRequestBuffer.Truncate();

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPRequest::Build() [this=%x].  Building Request string.\n",
            this));
    //
    // Build the request line...
    //
    // ie. Method SP Request-URI SP HTTP-Version CRLF
    //
    mRequestBuffer.Append(MethodToString(mMethod));

    if (mUsingProxy) {
        rv = mURI->GetSpec(getter_Copies(autoBuffer));
    } else {
        rv = mURI->GetPath(getter_Copies(autoBuffer));
    }
    mRequestBuffer.Append(autoBuffer);
    
    //Trim off the # portion if any...
    int refLocation = mRequestBuffer.RFind("#");
    if (-1 != refLocation)
        mRequestBuffer.Truncate(refLocation);

    mRequestBuffer.Append(" HTTP/1.0"CRLF);

/*    switch (mMethod)
    {
        case HM_GET:
            PL_strncpy(lineBuffer, MethodToString(mMethod)
            break;
        case HM_DELETE:
        case HM_HEAD:
        case HM_INDEX:
        case HM_LINK:
        case HM_OPTIONS:
        case HM_POST:
        case HM_PUT:
        case HM_PATCH:
        case HM_TRACE:
        case HM_UNLINK:
            NS_ERROR_NOT_IMPLEMENTED;
            break;
        default: NS_ERROR("No method set on request!");
            break;
    }
*/
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
                header->GetValue(getter_Copies(autoBuffer));

                headerAtom->ToString(autoString);
                mRequestBuffer.Append(autoString);

                mRequestBuffer.Append(": ");
                mRequestBuffer.Append(autoBuffer);
                mRequestBuffer.Append(CRLF);
            }
            enumerator->HasMoreElements(&bMoreHeaders);
        }
    }

    NS_ASSERTION(mConnection, "Connection disappeared!");

    // Currently nsIPostStreamData contains the header info and the data.
    // So we are forced to putting this here in the end. 
    // This needs to change! since its possible for someone to modify it
    // TODO- Gagan
    if (!mPostDataStream) {
        mRequestBuffer.Append(CRLF);
    }

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("\nnsHTTPRequest::Build() [this=%x].\tWriting Request:\n"
            "=== Start\n%s=== End\n",
            this, mRequestBuffer.GetBuffer()));

    //
    // Create a string stream to feed to the socket transport.  This stream
    // contains only the request.  Any POST data will be sent in a second
    // AsyncWrite to the server..
    //
    nsCOMPtr<nsISupports> result;
    nsCOMPtr<nsIInputStream> stream;

    rv = NS_NewCharInputStream(getter_AddRefs(result), 
                               mRequestBuffer.GetBuffer());
    if (NS_FAILED(rv)) return rv;

    stream = do_QueryInterface(result, &rv);
    if (NS_FAILED(rv)) return rv;

    //
    // Write the request to the server.  
    //
    rv = aTransport->AsyncWrite(stream, 0, mRequestBuffer.Length(), 
                                mConnection, this);
    return rv;
}

nsresult nsHTTPRequest::Clone(const nsHTTPRequest* *o_Request) const
{
    return NS_ERROR_FAILURE;
}
                        
nsresult nsHTTPRequest::SetMethod(HTTPMethod i_Method)
{
    mMethod = i_Method;
    return NS_OK;
}

HTTPMethod nsHTTPRequest::GetMethod(void) const
{
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


nsresult nsHTTPRequest::SetHTTPVersion(HTTPVersion i_Version)
{
    mVersion = i_Version;
    return NS_OK;
}

nsresult nsHTTPRequest::GetHTTPVersion(HTTPVersion* o_Version) 
{
    *o_Version = mVersion;
    return NS_OK;
}


nsresult nsHTTPRequest::SetPostDataStream(nsIInputStream* aStream)
{
  mPostDataStream = aStream;

  return NS_OK;
}

nsresult nsHTTPRequest::GetPostDataStream(nsIInputStream* *aResult)
{ 
  nsresult rv = NS_OK;

  if (aResult) {
    *aResult = mPostDataStream;
    NS_IF_ADDREF(*aResult);
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHTTPRequest::OnStartRequest(nsIChannel* channel, nsISupports* i_Context)
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPRequest [this=%x]. Starting to write data to the server.\n",
            this));
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPRequest::OnStopRequest(nsIChannel* channel, nsISupports* i_Context,
                             nsresult iStatus,
                             const PRUnichar* i_Msg)
{
    nsresult rv = iStatus;
    
    if (NS_SUCCEEDED(rv)) {
      //
      // Write the POST data out to the server...
      //
      if (mPostDataStream) {
        NS_ASSERTION(mMethod == HM_POST, "Post data without a POST method?");

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("nsHTTPRequest [this=%x]. Writing POST data to the server.\n",
                this));

        rv = mTransport->AsyncWrite(mPostDataStream, 0, -1, mConnection, this);

        /* the mPostDataStream is released below... */
      }
      //
      // Prepare to receive the response...
      //
      else {
        nsHTTPResponseListener* pListener;
        
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("nsHTTPRequest [this=%x]. Finished writing request to server."
                "\tStatus: %x\n", 
                this, iStatus));

        pListener = new nsHTTPResponseListener(mConnection);
        if (pListener) {
          NS_ADDREF(pListener);
          rv = mTransport->AsyncRead(0, -1, i_Context, pListener);
          NS_RELEASE(pListener);
        } else {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    //
    // An error occurred when trying to write the request to the server!
    //
    // Call the consumer OnStopRequest(...) to end the request...
    //
    else {
        nsCOMPtr<nsIStreamListener> consumer;
        nsCOMPtr<nsISupports> consumerContext;

        PR_LOG(gHTTPLog, PR_LOG_ERROR, 
               ("nsHTTPRequest [this=%x]. Error writing request to server."
                "\tStatus: %x\n", 
                this, iStatus));

        (void) mConnection->GetResponseContext(getter_AddRefs(consumerContext));
        rv = mConnection->GetResponseDataListener(getter_AddRefs(consumer));
        if (consumer) {
            consumer->OnStopRequest(mConnection, consumerContext, iStatus, i_Msg);
        }
        // Notify the channel that the request has finished
        mConnection->ResponseCompleted(mTransport, iStatus);
        mTransport = null_nsCOMPtr();

        rv = iStatus;
    }
 
    //
    // These resouces are no longer needed...
    //
    mRequestBuffer.Truncate();
    mPostDataStream = null_nsCOMPtr();

    return rv;
}


nsresult nsHTTPRequest::SetConnection(nsHTTPChannel* i_Connection)
{
    mConnection = i_Connection;
    return NS_OK;
}


nsresult nsHTTPRequest::GetHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mHeaders.GetEnumerator(aResult);
}


