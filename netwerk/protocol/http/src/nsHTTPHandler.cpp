/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *                          `
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
#include "nsHTTPHandler.h"
#include "nsHTTPChannel.h"

#include "nsIProxy.h"
#include "plstr.h" // For PL_strcasecmp maybe DEBUG only... TODO check
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsIEventSinkGetter.h"
#include "nsIHttpEventSink.h"
#include "nsIFileStream.h" 
#include "nsIStringStream.h" 
#include "nsHTTPEncodeStream.h" 
#include "nsHTTPAtoms.h"

#if defined(PR_LOGGING)
//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsHTTPProtocol:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gHTTPLog = nsnull;

#endif /* PR_LOGGING */


#define MAX_NUMBER_OF_OPEN_TRANSPORTS 8

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

NS_METHOD NS_CreateOrGetHTTPHandler(nsIHTTPProtocolHandler* *o_HTTPHandler)
{
#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for HTTP Protocol logging 
    // if necessary...
    //
    if (nsnull == gHTTPLog) {
        gHTTPLog = PR_NewLogModule("nsHTTPProtocol");
    }
#endif /* PR_LOGGING */
    
    if (o_HTTPHandler)
    {
        *o_HTTPHandler = nsHTTPHandler::GetInstance();
        return NS_OK;
    }
    return NS_ERROR_NULL_POINTER;
}

nsHTTPHandler::nsHTTPHandler()
{
    nsresult rv;
    NS_INIT_REFCNT();

    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Creating nsHTTPHandler [this=%x].\n", this));

    // Initialize the Atoms used by the HTTP protocol...
    nsHTTPAtoms::AddRefAtoms();

    rv = NS_NewISupportsArray(getter_AddRefs(m_pConnections));
    if (NS_FAILED(rv)) {
        NS_ERROR("unable to create new ISupportsArray");
    }

    rv = NS_NewISupportsArray(getter_AddRefs(mPendingChannelList));
    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to create the pending channel list");
    }

    rv = NS_NewISupportsArray(getter_AddRefs(mTransportList));
    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to create the transport list");
    }
}

nsHTTPHandler::~nsHTTPHandler()
{
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("Deleting nsHTTPHandler [this=%x].\n", this));

    mPendingChannelList->Clear();
    mTransportList->Clear();

    // Release the Atoms used by the HTTP protocol...
    nsHTTPAtoms::ReleaseAtoms();

}

NS_IMPL_ADDREF(nsHTTPHandler);

NS_METHOD
nsHTTPHandler::NewChannel(const char* verb, nsIURI* i_URL,
                          nsIEventSinkGetter *eventSinkGetter,
                          nsIChannel **o_Instance)
{
    nsresult rv;
    nsHTTPChannel* pChannel = nsnull;
    char* scheme        = nsnull;
    char* handlerScheme = nsnull;

    // Initial checks...
    if (!i_URL || !o_Instance) {
        return NS_ERROR_NULL_POINTER;
    }

    i_URL->GetScheme(&scheme);
    GetScheme(&handlerScheme);
    
    if (scheme != nsnull  && handlerScheme != nsnull  &&
        0 == PL_strcasecmp(scheme, handlerScheme)) 
    {
        if (scheme)
            nsCRT::free(scheme);
        
        if (handlerScheme)
            nsCRT::free(handlerScheme);
        
        nsCOMPtr<nsIURI> channelURI;
        PRUint32 count;
        PRInt32 index;

        //Check to see if an instance already exists in the active list
        m_pConnections->Count(&count);
        for (index=count-1; index >= 0; --index) {
            //switch to static_cast...
            pChannel = (nsHTTPChannel*)((nsIHTTPChannel*) m_pConnections->ElementAt(index));
            //Do other checks here as well... TODO
            rv = pChannel->GetURI(getter_AddRefs(channelURI));
            if (NS_SUCCEEDED(rv) && (channelURI.get() == i_URL))
            {
                NS_ADDREF(pChannel);
                *o_Instance = pChannel;
                return NS_OK; // TODO return NS_USING_EXISTING... or NS_DUPLICATE_REQUEST something like that.
            }
        }

        // Verify that the event sink is http
        nsCOMPtr<nsIHTTPEventSink>  httpEventSink;

        if (eventSinkGetter) {
            rv = eventSinkGetter->GetEventSink(verb, nsCOMTypeInfo<nsIHTTPEventSink>::GetIID(),
                                              (nsISupports**)(nsIHTTPEventSink**)getter_AddRefs(httpEventSink));
            if (NS_FAILED(rv)) return rv;
        }
        // Create one
        pChannel = new nsHTTPChannel(i_URL, 
                                     httpEventSink,
                                     this);
        if (pChannel) {
            NS_ADDREF(pChannel);
            pChannel->Init();
            rv = pChannel->QueryInterface(nsCOMTypeInfo<nsIChannel>::GetIID(), (void**)o_Instance);
            // add this instance to the active list of connections
            // TODO!
            NS_RELEASE(pChannel);
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
        return rv;
    }

    if (scheme)
            nsCRT::free(scheme);
        
    if (handlerScheme)
            nsCRT::free(handlerScheme);

    NS_ERROR("Non-HTTP request coming to HTTP Handler!!!");
    //return NS_ERROR_MISMATCHED_URL;
    return NS_ERROR_FAILURE;
}

nsresult
nsHTTPHandler::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    if (aIID.Equals(nsCOMTypeInfo<nsIProtocolHandler>::GetIID())) {
        *aInstancePtr = (void*) ((nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIHTTPProtocolHandler>::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIProxy>::GetIID())) {
        *aInstancePtr = (void*) ((nsIProxy*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = (void*) ((nsISupports*)(nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPHandler);

NS_METHOD
nsHTTPHandler::MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI,
                            char **_retval)
{
    // XXX optimize this to not needlessly construct the URL

    nsresult rv;
    nsIURI* url;
    rv = NewURI(aRelativeSpec, aBaseURI, &url);
    if (NS_FAILED(rv)) return rv;

    rv = url->GetSpec(_retval);
    NS_RELEASE(url);
    return rv;
}

NS_METHOD
nsHTTPHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                      nsIURI **result)
{
    //todo clean this up...
    nsresult rv;

    nsIURI* url;
    if (aBaseURI)
    {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else
    {
        rv = nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, nsCOMTypeInfo<nsIURI>::GetIID(), (void**)&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec((char*)aSpec);
    }
    if (NS_FAILED(rv)) return rv;

    nsIURI* realUrl = nsnull;
    
    rv = url->QueryInterface(nsCOMTypeInfo<nsIURI>::GetIID(), (void**)&realUrl);
    if (NS_FAILED(rv)) return rv;

    *result= realUrl;
    NS_ADDREF(*result);

    return rv;
}

NS_METHOD
nsHTTPHandler::FollowRedirects(PRBool bFollow)
{
    //m_bFollowRedirects = bFollow;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::NewEncodeStream(nsIInputStream *rawStream, PRUint32 encodeFlags,
                               nsIInputStream **result)
{
    return nsHTTPEncodeStream::Create(rawStream, encodeFlags, result);
}

NS_IMETHODIMP
nsHTTPHandler::NewDecodeStream(nsIInputStream *encodedStream, PRUint32 decodeFlags,
                               nsIInputStream **result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPHandler::NewPostDataStream(PRBool isFile, const char *data, PRUint32 encodeFlags,
                                 nsIInputStream **result)
{
    nsresult rv;
    if (isFile) {
        nsISupports* in;
        nsFileSpec spec(data);
        rv = NS_NewTypicalInputFileStream(&in, spec);
        if (NS_FAILED(rv)) return rv;

        nsIInputStream* rawStream;
        rv = in->QueryInterface(nsIInputStream::GetIID(), (void**)&rawStream);
        NS_RELEASE(in);
        if (NS_FAILED(rv)) return rv;

        rv = NewEncodeStream(rawStream, nsIHTTPProtocolHandler::ENCODE_NORMAL, result);
        NS_RELEASE(rawStream);
        return rv;
    }
    else {
        nsISupports* in;
        rv = NS_NewStringInputStream(&in, data);
        if (NS_FAILED(rv)) return rv;

        rv = in->QueryInterface(nsIInputStream::GetIID(), (void**)result);
        NS_RELEASE(in);
        return rv;
    }
}


nsresult nsHTTPHandler::RequestTransport(nsIURI* i_Uri, 
                                         nsHTTPChannel* i_Channel,
                                         nsIChannel** o_pTrans)
{
    nsresult rv;
    PRInt32 port;
    PRUint32 count;
    nsXPIDLCString host;

    *o_pTrans = nsnull;

    count = 0;
    mTransportList->Count(&count);
    if (count >= MAX_NUMBER_OF_OPEN_TRANSPORTS) {
        mPendingChannelList->AppendElement(i_Channel);

        PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
               ("nsHTTPHandler::RequestTransport."
                "\tAll socket transports are busy."
                "\tAdding nsHTTPChannel [%x] to pending list.\n",
                i_Channel));

        return NS_ERROR_BUSY;
    }

    // Get the host and port of the URI to create a new socket transport...
    rv = i_Uri->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    rv = i_Uri->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port == -1) {
        GetDefaultPort(&port);
    }


#if 0
    // Check in the table...
    nsIChannel* trans = (nsIChannel*) mTransportList->Get(&key);
    if (trans) {
        *o_pTrans = trans;
        return NS_OK;
    }
#endif /* 0 */

    // Create a new one...
    nsIChannel* trans;

    NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = sts->CreateTransport(host, port, &trans);
    if (NS_FAILED(rv)) return rv;

    // Put it in the table...
    mTransportList->AppendElement(trans);

    *o_pTrans = trans;
    
    PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
           ("nsHTTPHandler::RequestTransport."
            "\tGot a socket transport for nsHTTPChannel [%x].\n",
            i_Channel));

    return rv;
}

nsresult nsHTTPHandler::ReleaseTransport(nsIChannel* i_pTrans)
{
    nsresult rv;
    PRUint32 count;

    rv = mTransportList->RemoveElement(i_pTrans);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Transport not in table...");

    count = 0;
    mPendingChannelList->Count(&count);
    if (count) {
        nsCOMPtr<nsISupports> item;
        nsHTTPChannel* channel;

        rv = mPendingChannelList->GetElementAt(0, getter_AddRefs(item));
        if (NS_FAILED(rv)) return rv;

        mPendingChannelList->RemoveElement(item);
        channel = (nsHTTPChannel*)(nsISupports*)item;

        PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
               ("nsHTTPHandler::ReleaseTransport."
                "\tRestarting nsHTTPChannel [%x]\n",
                channel));

        channel->Open();
    }

    return rv;
}

