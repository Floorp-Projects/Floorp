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

#include "nsHTTPConnection.h"
#include "netCore.h"
#include "nsIHttpEventSink.h"
#include "nsIHTTPHandler.h"
#include "nsHTTPRequest.h"
#include "nsHTTPResponse.h"
#include "nsITransport.h"

nsHTTPConnection::nsHTTPConnection(
        nsIURL* i_URL, 
        nsIEventQueue* i_EQ, 
        nsIHTTPEventSink* i_HTTPEventSink,
        nsIHTTPHandler* i_Handler): 
    m_pURL( dont_QueryInterface(i_URL)),
    m_bConnected(PR_FALSE),
    m_State(HS_IDLE),
    mRefCnt(0),
    m_pHandler(dont_QueryInterface(i_Handler)),
    m_pEventSink(dont_QueryInterface(i_HTTPEventSink)),
    m_pResponse(nsnull),
    m_pEventQ(dont_QueryInterface(i_EQ))
    
{
    //TODO think if we need to make a copy of the URL and keep it here
    //since it might get deleted off the creators thread. And the
    //stream listener could be elsewhere...

    /* 
        Set up a request object - later set to a clone of a default 
        request from the handler
    */
    m_pRequest = new nsHTTPRequest(m_pURL);
    if (!m_pRequest)
        NS_ERROR("unable to create new nsHTTPRequest!");
    m_pRequest->SetConnection(this);
    
}

nsHTTPConnection::~nsHTTPConnection()
{
    //TODO if we keep our copy of m_pURL, then delete it too.
    if (m_pRequest)
    {
        delete m_pRequest;
        m_pRequest = 0;
    }
    if (m_pResponse)
    {
        delete m_pResponse;
        m_pResponse = 0;
    }
}

NS_IMPL_ADDREF(nsHTTPConnection);

NS_METHOD
nsHTTPConnection::Cancel(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::Suspend(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::Resume(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::GetInputStream(nsIInputStream* *o_Stream)
{
    if (!m_bConnected)
        Open();
    if (m_pResponse)
        return m_pResponse->GetInputStream(o_Stream);
    NS_ERROR("No response!");
    return NS_OK; // change to error ? or block till response is set up?

}

NS_METHOD
nsHTTPConnection::GetOutputStream(nsIOutputStream* *o_Stream)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::GetRequestHeader(const char* i_Header, const char* *o_Value) const
{
    NS_ASSERTION(m_pRequest, "The request object vanished from underneath the connection!");
    return m_pRequest->GetHeader(i_Header, o_Value);
}

NS_METHOD
nsHTTPConnection::SetRequestHeader(const char* i_Header, const char* i_Value)
{
    NS_ASSERTION(m_pRequest, "The request object vanished from underneath the connection!");
    return m_pRequest->SetHeader(i_Header, i_Value);
}

NS_METHOD
nsHTTPConnection::GetResponseHeader(const char* i_Header, const char* *o_Value)
{
    if (!m_bConnected)
        Open();
    if (m_pResponse)
        return m_pResponse->GetHeader(i_Header, o_Value);
    else
        return NS_ERROR_NOT_IMPLEMENTED; // NS_ERROR_NO_RESPONSE_YET ? 
}

// XXX jud
#if 0
static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);
static NS_DEFINE_IID(kINetModuleMgrIID, NS_INETMODULEMGR_IID);
static NS_DEFINE_CID(kCookieModuleCID, NS_COOKIEMODULE_CID);
#endif // jud

NS_METHOD
nsHTTPConnection::Open(void)
{
    if (m_bConnected || (m_State > HS_IDLE))
        return NS_ERROR_ALREADY_CONNECTED;

    // Set up a new request observer and a response listener and pass to teh transport
    nsresult rv = NS_OK;

    const char* host;
    rv = m_pURL->GetHost(&host);
    if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = m_pURL->GetPort(&port);
    if (NS_FAILED(rv)) return rv;
    nsITransport* temp;

    if (port == -1)
    {
        m_pHandler->GetDefaultPort(&port);
    }

    NS_ASSERTION(port>0, "Bad port setting!");
    PRUint32 unsignedPort = port;

// jud
#if 0
    // XXX this is not the right place for this
    // Check for any modules that want to set headers before we
    // send out a request.
    nsINetModuleMgr* pNetModuleMgr = nsnull;
    nsresult ret = nsServiceManager::GetService(kNetModuleMgrCID, kINetModuleMgrIID,
        (nsISupports**) &pNetModuleMgr);

    if (NS_SUCCEEDED(ret)) {
        nsIEnumerator* pModules = nsnull;
        ret = pNetModuleMgr->EnumerateModules("http-request", &pModules);
        if (NS_SUCCEEDED(ret)) ) {
            nsIProxyObjectManager*  proxyObjectManager = nsnull; 
            ret = nsComponentManager::CreateInstance(kProxyObjectManagerCID, 
                                               nsnull, 
                                               nsIProxyObjectManager::GetIID(), 
                                               (void**)&proxyObjectManager);
            nsNetModuleEntry *entry = nsnull;
            pModules->First(&entry);
            while (NS_SUCCEEDED(ret)) {
                // send the SetHeaders event to each registered module,
                // using the nsISupports Proxy service
                nsIHttpNotify *pNotify = nsnull;
                
                ret = proxyObjectManager->GetProxyObject(entry->mEventQ, 
                                                   kCookieModuleCID,
                                                   nsnull,
                                                   nsIHttpNotify::GetIID(),
                                                   (void**)&nsIHttpNotify);

                if (NS_SUCCEEDED(ret)) {
                    // send off the notification, and block.
                    ret = nsIHttpNotify->SetHeaders();
                    if (NS_SUCCEEDED(ret)) {
                        ;
                    }
                }

                pModules->Next(&entry);
  
            }
        }
    }
#endif // jud

    rv = m_pHandler->GetTransport(host, unsignedPort, &temp);
    if (temp)
    {
        m_pRequest->SetTransport(temp);

        nsIInputStream* stream;
        //Get the stream where it will read the request data from
        m_pRequest->GetInputStream(&stream);

        rv = temp->AsyncWrite(stream, (nsISupports*)(nsIProtocolConnection*)this , m_pEventQ, m_pRequest);

        m_State = HS_WAITING_FOR_RESPONSE;
        m_bConnected = PR_TRUE;
        NS_RELEASE(temp);
    }
    else
        NS_ERROR("Failed to create/get a transport!");

    return rv;
}

NS_METHOD
nsHTTPConnection::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIProtocolConnection::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsIProtocolConnection*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPConnection::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIHTTPConnection*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPConnection);

NS_METHOD
nsHTTPConnection::GetResponseStatus(PRInt32* o_Status)
{
    PRInt32 status = -1;
    if (!m_bConnected) 
        Open();
    *o_Status = status;

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::GetResponseString(const char* *o_String) 
{
    if (!m_bConnected) 
        Open();
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::SetRequestMethod(HTTPMethod i_Method)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::GetURL(nsIURL* *o_URL) const
{
    if (o_URL)
        *o_URL = m_pURL;
    return NS_OK;
}

