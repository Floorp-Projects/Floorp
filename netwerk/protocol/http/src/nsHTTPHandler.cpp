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
#include "nsHTTPHandler.h"
#include "nsHTTPChannel.h"

#include "nsIProxy.h"
#include "plstr.h" // For PL_strcasecmp maybe DEBUG only... TODO check
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHttpEventSink.h"
#include "nsIFileStream.h" 
#include "nsIStringStream.h" 
#include "nsHTTPEncodeStream.h" 
#include "nsHTTPAtoms.h"

#include "nsIPref.h" // preferences stuff
#ifdef DEBUG_gagan
#include "nsUnixColorPrintf.h"
#endif

#if defined(PR_LOGGING)
//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsHTTPProtocol:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gHTTPLog = nsnull;

#endif /* PR_LOGGING */

#define MAX_NUMBER_OF_OPEN_TRANSPORTS 8

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

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

nsHTTPHandler::nsHTTPHandler():
    mDoKeepAlive(PR_FALSE),
    mProxy(nsnull),
    mUseProxy(PR_FALSE)
{
    nsresult rv;
    NS_INIT_REFCNT();

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Creating nsHTTPHandler [this=%x].\n", this));

    // Initialize the Atoms used by the HTTP protocol...
    nsHTTPAtoms::AddRefAtoms();

    rv = NS_NewISupportsArray(getter_AddRefs(mConnections));
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
    
    // At some later stage we could merge this with the transport
    // list and add a field to each transport to determine its 
    // state. 
    rv = NS_NewISupportsArray(getter_AddRefs(mIdleTransports));
    if (NS_FAILED(rv)) {
        NS_ERROR("Failed to create the idle transport list");
    }

    // Prefs stuff. Is this the right place to do this? TODO check
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);

    if (NS_FAILED(rv)) 
    {
        NS_ERROR("Failed to access preferences service.");
        return;
    }
    else 
    {
#if 1   // only for keep alive
        // This stuff only till Keep-Alive is not switched on by default
        PRInt32 keepalive = -1;
        rv = prefs->GetIntPref("network.http.keep-alive", &keepalive);
        mDoKeepAlive = (keepalive == 1);
#ifdef DEBUG_gagan
        printf("Keep-alive switched ");
        printf(mDoKeepAlive ? STARTYELLOW "on\n" ENDCOLOR : 
                STARTRED "off\n" ENDCOLOR);
#endif //DEBUG_gagan
#endif  // remove till here

        nsXPIDLCString proxyServer;
        PRInt32 proxyPort = -1;
        PRInt32 type = -1;
        rv = prefs->GetIntPref("network.proxy.type", &type);
        //WARN for type==2
        mUseProxy = (type == 1); //type == 2 is autoconfig stuff. 
        if (NS_FAILED(rv))
            return ; // NE_ERROR("Failed to get proxy type");
        rv = prefs->CopyCharPref("network.proxy.http", 
                getter_Copies(proxyServer));
        if (NS_FAILED(rv)) 
            return; //NS_ERROR("Failed to get the HTTP proxy server");
        rv = prefs->GetIntPref("network.proxy.http_port",&proxyPort);

        if (NS_SUCCEEDED(rv) && (proxyPort>0)) // currently a bug in IntPref
        {
            if (NS_FAILED(SetProxyHost(proxyServer)))
                NS_ERROR("Failed to set the HTTP proxy server");
            if (NS_FAILED(SetProxyPort(proxyPort)))
                NS_ERROR("Failed to set the HTTP proxy port");
        }
    }
}

nsHTTPHandler::~nsHTTPHandler()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPHandler [this=%x].\n", this));

    mConnections->Clear();
    mIdleTransports->Clear();
    mPendingChannelList->Clear();
    mTransportList->Clear();

    // Release the Atoms used by the HTTP protocol...
    nsHTTPAtoms::ReleaseAtoms();

    CRTFREEIF(mProxy);

}

NS_METHOD
nsHTTPHandler::NewChannel(const char* verb, nsIURI* i_URL,
                          nsILoadGroup* aLoadGroup,
                          nsIInterfaceRequestor* notificationCallbacks,
                          nsLoadFlags loadAttributes,
                          nsIURI* originalURI,
                          PRUint32 bufferSegmentSize,
                          PRUint32 bufferMaxSize,
                          nsIChannel **o_Instance)
{
    nsresult rv;
    nsHTTPChannel* pChannel = nsnull;
    nsXPIDLCString scheme;
    nsXPIDLCString handlerScheme;

    // Initial checks...
    if (!i_URL || !o_Instance) {
        return NS_ERROR_NULL_POINTER;
    }

    i_URL->GetScheme(getter_Copies(scheme));
    GetScheme(getter_Copies(handlerScheme));
    
    if (scheme != nsnull  && handlerScheme != nsnull  &&
        0 == PL_strcasecmp(scheme, handlerScheme)) 
    {
        nsCOMPtr<nsIURI> channelURI;
        PRUint32 count;
        PRInt32 index;

        //Check to see if an instance already exists in the active list
        mConnections->Count(&count);
        for (index=count-1; index >= 0; --index) {
            //switch to static_cast...
            pChannel = (nsHTTPChannel*)((nsIHTTPChannel*) 
                mConnections->ElementAt(index));
            //Do other checks here as well... TODO
            rv = pChannel->GetURI(getter_AddRefs(channelURI));
            if (NS_SUCCEEDED(rv) && (channelURI.get() == i_URL))
            {
                NS_ADDREF(pChannel);
                *o_Instance = pChannel;
                // TODO return NS_USING_EXISTING... 
                // or NS_DUPLICATE_REQUEST something like that.
                return NS_OK; 
            }
        }

        // Create one
        pChannel = new nsHTTPChannel(i_URL, 
                                     verb,
                                     originalURI,
                                     this,
                                     bufferSegmentSize,
                                     bufferMaxSize);
        if (pChannel) {
            NS_ADDREF(pChannel);
            rv = pChannel->SetLoadAttributes(loadAttributes);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->SetLoadGroup(aLoadGroup);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->SetNotificationCallbacks(notificationCallbacks);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->QueryInterface(NS_GET_IID(nsIChannel), 
                                          (void**)o_Instance);
            // add this instance to the active list of connections
            // TODO!
          done:
            NS_RELEASE(pChannel);
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
        return rv;
    }

    NS_ERROR("Non-HTTP request coming to HTTP Handler!!!");
    //return NS_ERROR_MISMATCHED_URL;
    return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS3(nsHTTPHandler,
                   nsIHTTPProtocolHandler,
                   nsIProtocolHandler,
                   nsIProxy)

NS_METHOD
nsHTTPHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                      nsIURI **result)
{
    nsresult rv;

    nsIURI* url = nsnull;
    if (aBaseURI)
    {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else
    {
        rv = nsComponentManager::CreateInstance(kStandardUrlCID, 
                                    nsnull, NS_GET_IID(nsIURI),
                                    (void**)&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec((char*)aSpec);
    }
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }
    *result = url;
    return rv;
}

NS_METHOD
nsHTTPHandler::FollowRedirects(PRBool bFollow)
{
    //mFollowRedirects = bFollow;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::NewEncodeStream(nsIInputStream *rawStream, PRUint32 encodeFlags,
                               nsIInputStream **result)
{
    return nsHTTPEncodeStream::Create(rawStream, encodeFlags, result);
}

NS_IMETHODIMP
nsHTTPHandler::NewDecodeStream(nsIInputStream *encodedStream, 
                                PRUint32 decodeFlags,
                                nsIInputStream **result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPHandler::NewPostDataStream(PRBool isFile, 
                                const char *data, 
                                PRUint32 encodeFlags,
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

        rv = NewEncodeStream(rawStream,     
                nsIHTTPProtocolHandler::ENCODE_NORMAL, 
                result);
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
                                         PRUint32 bufferSegmentSize,
                                         PRUint32 bufferMaxSize,
                                         nsIChannel** o_pTrans)
{
    nsresult rv;
    PRUint32 count;

    *o_pTrans = nsnull;

    count = 0;
    mTransportList->Count(&count);
    if (count >= MAX_NUMBER_OF_OPEN_TRANSPORTS) {

        // XXX this method incorrectly returns a bool
        rv = mPendingChannelList->AppendElement((nsISupports*)(nsIRequest*)i_Channel) 
            ? NS_OK : NS_ERROR_FAILURE;  
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("nsHTTPHandler::RequestTransport."
                "\tAll socket transports are busy."
                "\tAdding nsHTTPChannel [%x] to pending list.\n",
                i_Channel));

        return NS_ERROR_BUSY;
    }

    PRInt32 port;
    nsXPIDLCString host;

    rv = i_Uri->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    rv = i_Uri->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port == -1)
        GetDefaultPort(&port);

    nsIChannel* trans;
    // Check in the idle transports for a host/port match
    count = 0;
    PRInt32 index = 0;
    if (mDoKeepAlive)
    {
        mIdleTransports->Count(&count);

        for (index=count-1; index >= 0; --index) 
        {
            nsCOMPtr<nsIURI> uri;
            trans = (nsIChannel*) mIdleTransports->ElementAt(index);
            if (trans && 
                    (NS_SUCCEEDED(trans->GetURI(getter_AddRefs(uri)))))
            {
                nsXPIDLCString idlehost;
                if (NS_SUCCEEDED(uri->GetHost(getter_Copies(idlehost))))
                {
                    if (0 == PL_strcasecmp(host, idlehost))
                    {
                        PRInt32 idleport;
                        if (NS_SUCCEEDED(uri->GetPort(&idleport)))
                        {
                            if (idleport == -1)
                                GetDefaultPort(&idleport);

#ifdef DEBUG_gagan
                            printf(STARTYELLOW "%s:%d\n", 
                                    (const char*)idlehost, idleport);
#endif

                            if (idleport == port)
                            {
                                // Addref it before removing it!
                                NS_ADDREF(trans);
#ifdef DEBUG_gagan
                                PRINTF_BLUE;
                                printf("Found a match in idle list!\n");
#endif
                                // Remove it from the idle
                                mIdleTransports->RemoveElement(trans);
                                //break;// break out of the for loop 
                            }
                        }
                    }
                }
            }
            // else delibrately ignored.
        }
    }
    // if we didn't find any from the keep-alive idlelist
    if (*o_pTrans == nsnull)
    {
        // Create a new one...
        if (!mProxy || !mUseProxy)
        {
            rv = CreateTransport(host, port, host,
                                 bufferSegmentSize, bufferMaxSize, &trans);
        }
        else
        {
            rv = CreateTransport(mProxy, mProxyPort, host, 
                                 bufferSegmentSize, bufferMaxSize, &trans);
        }
        if (NS_FAILED(rv)) return rv;
    }
    i_Channel->SetUsingProxy(mUseProxy);

    // Put it in the table...
    // XXX this method incorrectly returns a bool
    rv = mTransportList->AppendElement(trans) ? NS_OK : NS_ERROR_FAILURE;  
    if (NS_FAILED(rv)) return rv;

    *o_pTrans = trans;
    
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPHandler::RequestTransport."
            "\tGot a socket transport for nsHTTPChannel [%x]. %d Active transports.\n",
            i_Channel, count+1));

    return rv;
}

nsresult nsHTTPHandler::CreateTransport(const char* host, 
                                        PRInt32 port, 
                                        const char* aPrintHost,
                                        PRUint32 bufferSegmentSize,
                                        PRUint32 bufferMaxSize,
                                        nsIChannel** o_pTrans)
{
    nsresult rv;

    NS_WITH_SERVICE(nsISocketTransportService, sts, 
                    kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    return sts->CreateTransport(host, port, aPrintHost,
                                bufferSegmentSize, bufferMaxSize,
                                o_pTrans);
}

nsHTTPHandler * nsHTTPHandler::GetInstance(void)
{
    static nsHTTPHandler* pHandler = new nsHTTPHandler();
    NS_IF_ADDREF(pHandler);
    return pHandler;
};


nsresult nsHTTPHandler::ReleaseTransport(nsIChannel* i_pTrans)
{
    nsresult rv;
    PRUint32 count=0;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPHandler::ReleaseTransport."
            "\tReleasing socket transport %x.\n",
            i_pTrans));
    //
    // Clear the EventSinkGetter for the transport...  This breaks the
    // circular reference between the HTTPChannel which holds a reference
    // to the transport and the transport which references the HTTPChannel
    // through the event sink...
    //
    rv = i_pTrans->SetNotificationCallbacks(nsnull);

    rv = mTransportList->RemoveElement(i_pTrans);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Transport not in table...");

    if (mDoKeepAlive)
    {
        // if this transport is to be kept alive 
        // then add it to mIdleTransports
        NS_WITH_SERVICE(nsISocketTransportService, sts, 
                kSocketTransportServiceCID, &rv);
        // don't bother about failure of this one...
        if (NS_SUCCEEDED(rv))
        {
            PRBool reuse = PR_FALSE;
            rv = sts->ReuseTransport(i_pTrans, &reuse);
            // Delibrately ignoring the return value of AppendElement 
            // if we can't append a transport we just won't have keep-alive
            // but life goes on... assert in debug though
            if (NS_SUCCEEDED(rv) && reuse)
            {
                PRBool added = mIdleTransports->AppendElement(i_pTrans);
                NS_ASSERTION(added, 
                    "Failed to add a socket to idle transports list!");
            }
        }
    }
    
    // Now trigger an additional one from the pending list
    mPendingChannelList->Count(&count);
    if (count) {
        nsCOMPtr<nsISupports> item;
        nsHTTPChannel* channel;

        rv = mPendingChannelList->GetElementAt(0, getter_AddRefs(item));
        if (NS_FAILED(rv)) return rv;

        mPendingChannelList->RemoveElement(item);
        channel = (nsHTTPChannel*)(nsIRequest*)(nsISupports*)item;

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("nsHTTPHandler::ReleaseTransport."
                "\tRestarting nsHTTPChannel [%x]\n",
                channel));

        channel->Open();
    }

    return rv;
}

nsresult nsHTTPHandler::CancelPendingChannel(nsHTTPChannel* aChannel)
{
  PRBool ret;

  // XXX: RemoveElement *really* returns a PRBool :-(
  ret = (PRBool) mPendingChannelList->RemoveElement((nsISupports*)(nsIRequest*)aChannel);

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPHandler::CancelPendingChannel."
          "\tCancelling nsHTTPChannel [%x]\n",
          aChannel));

  return ret ? NS_OK : NS_ERROR_FAILURE;

}


nsresult
nsHTTPHandler::GetProxyHost(const char* *o_ProxyHost) const
{
    if (!o_ProxyHost)
        return NS_ERROR_NULL_POINTER;
    if (mProxy)
    {
        *o_ProxyHost = nsCRT::strdup(mProxy);
        return (*o_ProxyHost == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_ProxyHost = nsnull;
        return NS_OK;
    }
}

nsresult
nsHTTPHandler::SetProxyHost(const char* i_ProxyHost) 
{
    CRTFREEIF(mProxy);
    if (i_ProxyHost)
    {
        mProxy = nsCRT::strdup(i_ProxyHost);
        return (mProxy == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    return NS_OK;
}
