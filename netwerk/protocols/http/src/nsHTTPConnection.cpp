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
#include "nsIHTTPEventSink.h"
#include "nsIHTTPHandler.h"
#include "nsHTTPRequest.h"
#include "nsHTTPResponse.h"

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
    return NS_ERROR_NOT_IMPLEMENTED;
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

NS_METHOD
nsHTTPConnection::Open(void)
{
    if (m_bConnected || (m_State > HS_IDLE))
        return NS_ERROR_ALREADY_CONNECTED;

    m_bConnected = PR_TRUE;

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPConnection::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIProtocolConnection::GetIID())) {
        *aInstancePtr = (void*) ((nsIProtocolConnection*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPConnection::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPConnection*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)(nsIProtocolConnection*)this);
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

