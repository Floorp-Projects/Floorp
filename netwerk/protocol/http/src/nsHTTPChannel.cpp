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
#include "nsHTTPChannel.h"
#include "netCore.h"
#include "nsIHttpEventSink.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPRequest.h"
#include "nsHTTPResponse.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIIOService.h"
#include "nsXPIDLString.h"
#include "nsHTTPAtoms.h"

#include "nsIHttpNotify.h"
#include "nsINetModRegEntry.h"
#include "nsIServiceManager.h"
#include "nsINetModuleMgr.h"
#include "nsIEventQueueService.h"
#include "nsIMIMEService.h"
#include "nsIEnumerator.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);

nsHTTPChannel::nsHTTPChannel(nsIURI* i_URL, 
                             nsIHTTPEventSink* i_HTTPEventSink,
                             nsHTTPHandler* i_Handler): 
    m_URI(dont_QueryInterface(i_URL)),
    m_bConnected(PR_FALSE),
    m_State(HS_IDLE),
    mRefCnt(0),
    m_pHandler(dont_QueryInterface(i_Handler)),
    m_pEventSink(dont_QueryInterface(i_HTTPEventSink)),
    m_pResponse(nsnull),
    m_pResponseDataListener(nsnull),
    mLoadAttributes(LOAD_NORMAL),
    mResponseContext(nsnull),
    mLoadGroup(nsnull),
    mPostStream(nsnull)
{
    NS_INIT_REFCNT();

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Creating nsHTTPChannel [this=%x].\n", this));


}

nsHTTPChannel::~nsHTTPChannel()
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Deleting nsHTTPChannel [this=%x].\n", this));

    //TODO if we keep our copy of m_URI, then delete it too.
    NS_IF_RELEASE(m_pRequest);
    NS_IF_RELEASE(m_pResponse);
    NS_IF_RELEASE(mPostStream);
    NS_IF_RELEASE(m_pResponseDataListener);
    NS_IF_RELEASE(mLoadGroup);
}

NS_IMETHODIMP
nsHTTPChannel::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    if (aIID.Equals(nsCOMTypeInfo<nsIHTTPChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIHTTPChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_ADDREF(nsHTTPChannel);
NS_IMPL_RELEASE(nsHTTPChannel);

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsHTTPChannel::IsPending(PRBool *result)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (m_pRequest) {
    rv = m_pRequest->IsPending(result);
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPChannel::Cancel(void)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (m_pRequest) {
    rv = m_pRequest->Cancel();
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPChannel::Suspend(void)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (m_pRequest) {
    rv = m_pRequest->Suspend();
  }
  return rv;
}

NS_IMETHODIMP
nsHTTPChannel::Resume(void)
{
  nsresult rv = NS_ERROR_NULL_POINTER;

  if (m_pRequest) {
    rv = m_pRequest->Resume();
  }
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsHTTPChannel::GetURI(nsIURI* *o_URL)
{
    if (o_URL) {
        *o_URL = m_URI;
        NS_IF_ADDREF(*o_URL);
        return NS_OK;
    } else {
        return NS_ERROR_NULL_POINTER;
    }
}

NS_IMETHODIMP
nsHTTPChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **o_Stream)
{
#if 0
    nsresult rv;

    if (!m_bConnected)
        Open();

    nsIInputStream* inStr; // this guy gets passed out to the user
    rv = NS_NewSyncStreamListener(&m_pResponseDataListener, &inStr);
    if (NS_FAILED(rv)) return rv;

    *o_Stream = inStr;
    return NS_OK;

#else

    if (m_pResponse)
        return m_pResponse->GetInputStream(o_Stream);
    NS_ERROR("No response!");
    return NS_OK; // change to error ? or block till response is set up?
#endif // if 0

}

NS_IMETHODIMP
nsHTTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *aContext,
                         nsIStreamListener *listener)
{
    nsresult rv = NS_OK;

    // Initial parameter checks...
    if (m_pResponseDataListener) {
        rv = NS_ERROR_IN_PROGRESS;
    } 
    else if (!listener) {
        rv = NS_ERROR_NULL_POINTER;
    }

    // Initiate the loading of the URL...
    if (NS_SUCCEEDED(rv)) {
        m_pResponseDataListener = listener;
        NS_ADDREF(m_pResponseDataListener);

        mResponseContext = aContext;

        rv = Open();
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition,
                          PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

#define DUMMY_TYPE "text/html"

NS_IMETHODIMP
nsHTTPChannel::GetContentType(char * *aContentType)
{
    nsresult rv = NS_OK;

    // Parameter validation...
    if (!aContentType) {
        return NS_ERROR_NULL_POINTER;
    }
    *aContentType = nsnull;

    //
    // If the content type has been returned by the server then retern that...
    //
    if (mContentType.Length()) {
        *aContentType = mContentType.ToNewCString();
        if (!*aContentType) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
        return rv;
    }

    //
    // No response yet...  Try to determine the content-type based
    // on the file extension of the URI...
    //
    NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = MIMEService->GetTypeFromURI(m_URI, aContentType);
        if (NS_SUCCEEDED(rv)) return rv;
        // XXX we should probably set the content-type for this http response at this stage too.
    }

    // if all else fails treat it as text/html?
	if (!*aContentType) 
		*aContentType = nsCRT::strdup(DUMMY_TYPE);
    if (!*aContentType) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
    NS_IF_RELEASE(mLoadGroup);
    mLoadGroup = aLoadGroup;
    NS_IF_ADDREF(mLoadGroup);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIHTTPChannel methods:

NS_IMETHODIMP
nsHTTPChannel::GetRequestHeader(nsIAtom* i_Header, char* *o_Value)
{
    NS_ASSERTION(m_pRequest, "The request object vanished from underneath the connection!");
    return m_pRequest->GetHeader(i_Header, o_Value);
}

NS_IMETHODIMP
nsHTTPChannel::SetRequestHeader(nsIAtom* i_Header, const char* i_Value)
{
    NS_ASSERTION(m_pRequest, "The request object vanished from underneath the connection!");
    return m_pRequest->SetHeader(i_Header, i_Value);
}

NS_IMETHODIMP
nsHTTPChannel::GetRequestHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    nsresult rv;

    if (m_pRequest) {
        rv = m_pRequest->GetHeaderEnumerator(aResult);
    } else {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}


NS_IMETHODIMP
nsHTTPChannel::GetResponseHeader(nsIAtom* i_Header, char* *o_Value)
{
    if (!m_bConnected)
        Open();
    if (m_pResponse)
        return m_pResponse->GetHeader(i_Header, o_Value);
    else
        return NS_ERROR_FAILURE ; // NS_ERROR_NO_RESPONSE_YET ? 
}


NS_IMETHODIMP
nsHTTPChannel::GetResponseHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    nsresult rv;

    if (!m_bConnected) {
        Open();
    }

    if (m_pResponse) {
        rv = m_pResponse->GetHeaderEnumerator(aResult);
    } else {
        *aResult = nsnull;
        rv = NS_ERROR_FAILURE ; // NS_ERROR_NO_RESPONSE_YET ? 
    }
    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::GetResponseStatus(PRUint32  *o_Status)
{
    if (!m_bConnected) 
        Open();
	if (m_pResponse)
		return m_pResponse->GetStatus(o_Status);
    return NS_ERROR_FAILURE; // NS_ERROR_NO_RESPONSE_YET ? 
}

NS_IMETHODIMP
nsHTTPChannel::GetResponseString(char* *o_String) 
{
    if (!m_bConnected) 
        Open();
	if (m_pResponse)
		return m_pResponse->GetStatusString(o_String);
    return NS_ERROR_FAILURE; // NS_ERROR_NO_RESPONSE_YET ? 
}

NS_IMETHODIMP
nsHTTPChannel::GetEventSink(nsIHTTPEventSink* *o_EventSink) 
{
    nsresult rv;

    if (o_EventSink) {
        *o_EventSink = m_pEventSink;
        NS_IF_ADDREF(*o_EventSink);
        rv = NS_OK;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}


NS_IMETHODIMP
nsHTTPChannel::SetRequestMethod(PRUint32/*HTTPMethod*/ i_Method)
{
    NS_ASSERTION(m_pRequest, "No request set as yet!");
    return m_pRequest ? m_pRequest->SetMethod((HTTPMethod)i_Method) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTTPChannel::GetResponseDataListener(nsIStreamListener* *aListener)
{
    nsresult rv = NS_OK;

    if (aListener) {
        *aListener = m_pResponseDataListener;
        NS_IF_ADDREF(m_pResponseDataListener);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);


////////////////////////////////////////////////////////////////////////////////
// nsHTTPChannel methods:

nsresult
nsHTTPChannel::Init()
{
    //TODO think if we need to make a copy of the URL and keep it here
    //since it might get deleted off the creators thread. And the
    //stream listener could be elsewhere...

    /* 
        Set up a request object - later set to a clone of a default 
        request from the handler
    */
    nsresult rv;

    m_pRequest = new nsHTTPRequest(m_URI);
    if (m_pRequest == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(m_pRequest);
    m_pRequest->SetConnection(this);
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    PRUnichar * ua = nsnull;
    rv = service->GetUserAgent(&ua);
    if (NS_FAILED(rv)) return rv;
    nsString2 uaString(ua);
    nsCRT::free(ua);
    char * uaCString = uaString.ToNewCString();
    if (!uaCString) return NS_ERROR_OUT_OF_MEMORY;
    m_pRequest->SetHeader(nsHTTPAtoms::User_Agent, uaCString);
    nsCRT::free(uaCString);
    return NS_OK;
}

nsresult
nsHTTPChannel::Open(void)
{
    if (m_bConnected || (m_State > HS_WAITING_FOR_OPEN))
        return NS_ERROR_ALREADY_CONNECTED;

    // Set up a new request observer and a response listener and pass to the transport
    nsresult rv = NS_OK;
    nsCOMPtr<nsIChannel> channel;

    rv = m_pHandler->RequestTransport(m_URI, this, getter_AddRefs(channel));
    if (NS_ERROR_BUSY == rv) {
        m_State = HS_WAITING_FOR_OPEN;
        return NS_OK;
    }

    // Check for any modules that want to set headers before we
    // send out a request.
    NS_WITH_SERVICE(nsINetModuleMgr, pNetModuleMgr, kNetModuleMgrCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> pModules;
    rv = pNetModuleMgr->EnumerateModules(NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_PROGID, getter_AddRefs(pModules));
    if (NS_FAILED(rv)) return rv;

    // Go through the external modules and notify each one.
    nsISupports *supEntry;
    rv = pModules->GetNext(&supEntry);
    while (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsINetModRegEntry> entry = do_QueryInterface(supEntry, &rv);
        if (NS_FAILED(rv)) 
            return rv;

        nsCOMPtr<nsINetNotify> syncNotifier;
        entry->GetSyncProxy(getter_AddRefs(syncNotifier));
        nsCOMPtr<nsIHTTPNotify> pNotify = do_QueryInterface(syncNotifier, &rv);

        if (NS_SUCCEEDED(rv)) 
        {
            // send off the notification, and block.
            // make the nsIHTTPNotify api call
            pNotify->ModifyRequest(this);
            // we could do something with the return code from the external
            // module, but what????            
        }
        rv = pModules->GetNext(&supEntry); // go around again
    }
    
    if (channel) {
        nsCOMPtr<nsIInputStream> stream;

        m_pRequest->SetTransport(channel);

        //Get the stream where it will read the request data from
        rv = m_pRequest->GetInputStream(getter_AddRefs(stream));
        if (NS_SUCCEEDED(rv) && stream) {
            PRUint32 count;

            // Write the request to the server...
            rv = stream->GetLength(&count);
            rv = channel->AsyncWrite(stream, 0, count, this , m_pRequest);
            if (NS_FAILED(rv)) return rv;

            m_State = HS_WAITING_FOR_RESPONSE;
            m_bConnected = PR_TRUE;
        } else {
            NS_ERROR("Failed to get request Input stream.");
            return NS_ERROR_FAILURE;
        }
    }
    else
        NS_ERROR("Failed to create/get a transport!");

    return rv;
}

nsresult nsHTTPChannel::ResponseCompleted(nsIChannel* aTransport)
{
  // Null out pointers that are no longer needed...
  mResponseContext = null_nsCOMPtr();
  NS_IF_RELEASE(m_pResponseDataListener);

  return m_pHandler->ReleaseTransport(aTransport);
}

nsresult nsHTTPChannel::SetResponse(nsHTTPResponse* i_pResp)
{ 
  NS_IF_RELEASE(m_pResponse);
  m_pResponse = i_pResp;
  NS_IF_ADDREF(m_pResponse);

  return NS_OK;
}

nsresult nsHTTPChannel::GetResponseContext(nsISupports** aContext)
{
  if (aContext) {
    *aContext = mResponseContext;
    NS_IF_ADDREF(*aContext);
    return NS_OK;
  }

  return NS_ERROR_NULL_POINTER;
}

nsresult nsHTTPChannel::SetContentType(const char* aContentType)
{
    mContentType = aContentType;
    return NS_OK;
}


nsresult
nsHTTPChannel::SetPostDataStream(nsIInputStream* postDataStream)
{
    NS_IF_RELEASE(mPostStream);
    mPostStream = postDataStream;
    if (mPostStream)
        NS_ADDREF(mPostStream);
    return NS_OK;
}

nsresult
nsHTTPChannel::GetPostDataStream(nsIInputStream **o_postStream)
{ 
    if (o_postStream)
    {
        *o_postStream = mPostStream; 
        NS_IF_ADDREF(*o_postStream); 
        return NS_OK; 
    }
    return NS_ERROR_NULL_POINTER;
}
