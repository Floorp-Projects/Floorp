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

#include "nsHTTPHandler.h"
#include "nsHTTPConnection.h"
#include "nsITimer.h" 
#include "nsIProxy.h"
#include "plstr.h" // For PL_strcasecmp maybe DEBUG only... TODO check
#include "nsIUrl.h"
#include "nsSocketKey.h"
#include "nsITransport.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsIHTTPEventSink.h"

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

NS_METHOD CreateOrGetHTTPHandler(nsIHTTPHandler* *o_HTTPHandler)
{
    if (o_HTTPHandler)
    {
        *o_HTTPHandler = nsHTTPHandler::GetInstance();
        return NS_OK;
    }
    return NS_ERROR_NULL_POINTER;
}

nsHTTPHandler::nsHTTPHandler():
    m_pTransportTable(nsnull),
    mRefCnt(0)
{
    if (NS_FAILED(NS_NewISupportsArray(getter_AddRefs(m_pConnections)))) {
        NS_ERROR("unable to create new ISupportsArray");
    }
}

nsHTTPHandler::~nsHTTPHandler()
{
    if (m_pTransportTable)
    {
        delete m_pTransportTable;
        m_pTransportTable = 0;
    }
}

NS_IMPL_ADDREF(nsHTTPHandler);

NS_METHOD
nsHTTPHandler::NewConnection(nsIURL* i_URL,
                                nsISupports* i_eventSink,
                                nsIEventQueue* i_eventQueue,
                                nsIProtocolConnection* *o_Instance)
{
    //Assert that iURL's scheme is HTTP
    //This should only happen in debug checks... TODO
    const char* scheme = 0;
    if (i_URL)
    {
        i_URL->GetScheme(&scheme);
        if (0 == PL_strcasecmp(scheme, "http"))
        {
            nsHTTPConnection* pConn = nsnull;
            nsIURL* pURL = nsnull;
            //Check to see if an instance already exists in the active list
            PRUint32 count;
            PRInt32 index;
            m_pConnections->Count(&count);
            for (index=count-1; index >= 0; --index) 
            {
                //switch to static_cast...
                pConn = (nsHTTPConnection*)((nsIHTTPConnection*) m_pConnections->ElementAt(index));
                //Do other checks here as well... TODO
                if ((NS_OK == pConn->GetURL(&pURL)) && (pURL == i_URL))
                {
                    NS_ADDREF(pConn);
                    *o_Instance = pConn;
                    return NS_OK; // TODO return NS_USING_EXISTING... or NS_DUPLICATE_REQUEST something like that.
                }
            }

            // Verify that the event sink is http
            nsCOMPtr<nsIHTTPEventSink>  httpEventSink (do_QueryInterface(i_eventSink));

            // Create one
            nsHTTPConnection* pNewInstance = new nsHTTPConnection(
                    i_URL, 
                    i_eventQueue,
                    httpEventSink,
                    this);
            if (pNewInstance)
            {
                NS_ADDREF(pNewInstance);
                *o_Instance = pNewInstance;
                // add this instance to the active list of connections
                return NS_OK;
            }
            else
                return NS_ERROR_OUT_OF_MEMORY;
        }
        NS_ASSERTION(0, "Non-HTTP request coming to HTTP Handler!!!");
        //return NS_ERROR_MISMATCHED_URL;
    }
    return NS_ERROR_NULL_POINTER;
}

nsresult
nsHTTPHandler::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIProtocolHandler::GetIID())) {
        *aInstancePtr = (void*) ((nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPHandler::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIProxy::GetIID())) {
        *aInstancePtr = (void*) ((nsIProxy*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)(nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPHandler);

NS_METHOD
nsHTTPHandler::MakeAbsoluteUrl(const char* i_URL,
                        nsIURL* i_BaseURL,
                        char* *o_Result) const
{
    return NS_OK;
}

NS_METHOD
nsHTTPHandler::NewUrl(const char* i_URL,
                        nsIURL* *o_Result,
                        nsIURL* i_BaseURL) const
{
    //todo clean this up...
    nsresult rv;

    nsIUrl* url;
    rv = nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, nsIUrl::GetIID(), (void**)&url);
    if (NS_FAILED(rv)) return rv;

    rv = url->Init(i_URL, i_BaseURL);

    nsIUrl* realUrl = nsnull;
    
    rv = url->QueryInterface(nsIUrl::GetIID(), (void**)&realUrl);
    if (NS_FAILED(rv)) return rv;

    *o_Result= realUrl;
    NS_ADDREF(*o_Result);

    return rv;
}

NS_METHOD
nsHTTPHandler::GetTransport(const char* i_Host, 
                            PRUint32& i_Port, 
                            nsITransport** o_pTrans)
{
    // Check in the table...
    nsSocketKey key(i_Host, i_Port);
    nsITransport* trans = (nsITransport*) m_pTransportTable->Get(&key);
    if (trans)
    {
        *o_pTrans = trans;
        return NS_OK;
    }

    // Create a new one...
    nsresult rv;

    NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = sts->CreateTransport(i_Host, i_Port, &trans);
    if (NS_FAILED(rv)) return rv;

    // Put it in the table...
    void* oldValue = m_pTransportTable->Put(&key, trans);
    NS_ASSERTION(oldValue == nsnull, "Race condition in transport table!");
    NS_ADDREF(trans);

    return rv;
}

NS_METHOD
nsHTTPHandler::ReleaseTransport(const char* i_Host, 
                                PRUint32& i_Port, 
                                nsITransport* i_pTrans)
{
    nsSocketKey key(i_Host, i_Port);
    nsITransport* value = (nsITransport*) m_pTransportTable->Remove(&key);
    if (value == nsnull)
        return NS_ERROR_FAILURE;
    NS_ASSERTION(i_pTrans == value, "m_pTransportTable is out of sync");
    return NS_OK;
}

NS_METHOD
nsHTTPHandler::FollowRedirects(PRBool bFollow)
{
    //m_bFollowRedirects = bFollow;
    return NS_OK;
}