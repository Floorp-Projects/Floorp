/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIOService.h"
#include "nsIProtocolHandler.h"
#include "nscore.h"
#include "nsString2.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIFileTransportService.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsCOMPtr.h"
#include "prprf.h"
#include "prmem.h"      // for PR_Malloc
#include <ctype.h>      // for isalpha
#include "nsIFileProtocolHandler.h"     // for NewChannelFromNativePath
#include "nsLoadGroup.h"
#include "nsIFileChannel.h"
#include "nsInputStreamChannel.h"
#include "nsXPIDLString.h" 

static NS_DEFINE_CID(kFileTransportService, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsIOService::nsIOService()
{
    NS_INIT_REFCNT();
}

nsresult
nsIOService::Init()
{
    return NS_OK;
}

nsIOService::~nsIOService()
{
}

NS_METHOD
nsIOService::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsIOService* _ios = new nsIOService();
    if (_ios == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(_ios);
    nsresult rv = _ios->QueryInterface(aIID, aResult);
    NS_RELEASE(_ios);
    return rv;
}

NS_IMPL_ISUPPORTS(nsIOService, nsCOMTypeInfo<nsIIOService>::GetIID());

////////////////////////////////////////////////////////////////////////////////

#define MAX_SCHEME_LENGTH       64      // XXX big enough?

#define MAX_NET_PROGID_LENGTH   (MAX_SCHEME_LENGTH + NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH + 1)

NS_IMETHODIMP
nsIOService::GetProtocolHandler(const char* scheme, nsIProtocolHandler* *result)
{
    nsresult rv;

    NS_ASSERTION(NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH
                 == nsCRT::strlen(NS_NETWORK_PROTOCOL_PROGID_PREFIX),
                 "need to fix NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH");

    // XXX we may want to speed this up by introducing our own protocol 
    // scheme -> protocol handler mapping, avoiding the string manipulation
    // and service manager stuff

    char buf[MAX_NET_PROGID_LENGTH];
    nsAutoString2 progID(NS_NETWORK_PROTOCOL_PROGID_PREFIX);
    progID += scheme;
    progID.ToCString(buf, MAX_NET_PROGID_LENGTH);

    NS_WITH_SERVICE(nsIProtocolHandler, handler, buf, &rv);
    if (NS_FAILED(rv)) return rv;

    *result = handler;
	NS_ADDREF(handler);
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::NewURI(const char* aSpec, nsIURI* aBaseURI,
                    nsIURI* *result)
{
    nsresult rv;

	nsAutoString scheme(aSpec);
	scheme.CompressWhitespace(); // remove any leading/trailing spaces;
	int firstColon = scheme.Find(":");
	if (-1 == firstColon)
	{
		if (aBaseURI) 
		{
			nsXPIDLCString sch;
			rv = aBaseURI->GetScheme(getter_Copies(sch));
			if (NS_FAILED(rv) || !sch || (0 == nsCRT::strlen(sch)))
				return NS_ERROR_MALFORMED_URI;
			scheme = sch;
		}
		else
			return NS_ERROR_MALFORMED_URI; // Must have a scheme
	}
	else 
		scheme.Truncate(firstColon);

	// Use the scheme to find the handler and ask it to create a new URI
	nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler(nsCAutoString(scheme), 
		getter_AddRefs(handler));
   	if (NS_FAILED(rv))
	{
		// Didn't find any handler for this scheme
		return rv; 
	}
    return handler->NewURI(aSpec, aBaseURI, result);
}

NS_IMETHODIMP
nsIOService::NewChannelFromURI(const char* verb, nsIURI *aURI,
                               nsIEventSinkGetter *eventSinkGetter,
                               nsIChannel **result)
{
    nsresult rv;

    nsXPIDLCString scheme;
    rv = aURI->GetScheme(getter_Copies(scheme));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler((const char*)scheme, getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQ;
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), 
                                            getter_AddRefs(eventQ));
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = handler->NewChannel(verb, aURI, eventSinkGetter, eventQ,
                             &channel);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

NS_IMETHODIMP
nsIOService::NewChannel(const char* verb, const char *aSpec,
                        nsIURI *aBaseURI,
                        nsIEventSinkGetter *eventSinkGetter,
                        nsIChannel **result)
{
    nsresult rv;
    nsIURI* uri;
    rv = NewURI(aSpec, aBaseURI, &uri);
    if (NS_FAILED(rv)) return rv;
    rv = NewChannelFromURI(verb, uri, eventSinkGetter, result);
    NS_RELEASE(uri);
    return rv;
}

NS_IMETHODIMP
nsIOService::MakeAbsolute(const char *aSpec,
                          nsIURI *aBaseURI,
                          char **result)
{
    nsresult rv;
    NS_ASSERTION(aBaseURI, "It doesn't make sense to not supply a base URI");
	if (!aBaseURI) 
		return NS_ERROR_FAILURE;

	if (aSpec == nsnull)
		return aBaseURI->GetSpec(result);
	
	nsAutoString scheme(aSpec);
	scheme.CompressWhitespace(); // remove any leading/trailing spaces;
	int firstColon = scheme.Find(":");
	if (-1 != firstColon)
	{
		// if aSpec has a scheme, then it's already absolute
		*result = nsCRT::strdup(aSpec);
		return (*result == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
	}

	// else ask the protocol handler for the base URI to deal with it
    nsXPIDLCString baseScheme;
    rv = aBaseURI->GetScheme(getter_Copies(baseScheme));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler((const char*)baseScheme, 
		getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    return handler->MakeAbsolute(aSpec, aBaseURI, result);
}

NS_IMETHODIMP
nsIOService::GetAppCodeName(PRUnichar* *aAppCodeName)
{
    *aAppCodeName = mAppCodeName.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetAppVersion(PRUnichar* *aAppVersion)
{
    *aAppVersion = mAppVersion.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetAppName(PRUnichar* *aAppName)
{
    *aAppName = mAppName.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetLanguage(PRUnichar* *aLanguage)
{
    *aLanguage = mAppLanguage.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetPlatform(PRUnichar* *aPlatform)
{
    *aPlatform = mAppPlatform.ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::GetUserAgent(PRUnichar* *aUserAgent)
{
    // XXX this should load the http module and ask for the user agent string from it.
    char buf[200];
    PR_snprintf(buf, 200, "%.100s/%.90s", mAppCodeName.GetBuffer(), mAppVersion.GetBuffer());
    nsAutoString2 aUA(buf);
    *aUserAgent = aUA.ToNewUnicode();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsIOService::NewAsyncStreamObserver(nsIStreamObserver *receiver, nsIEventQueue *eventQueue,
                                    nsIStreamObserver **result)
{
    return NS_NewAsyncStreamObserver(result, eventQueue, receiver);
    
}

NS_IMETHODIMP
nsIOService::NewAsyncStreamListener(nsIStreamListener *receiver, nsIEventQueue *eventQueue,
                                    nsIStreamListener **result)
{
    return NS_NewAsyncStreamListener(result, eventQueue, receiver);

}

NS_IMETHODIMP
nsIOService::NewSyncStreamListener(nsIInputStream **inStream, 
                                   nsIBufferOutputStream **outStream,
                                   nsIStreamListener **listener)
{
    return NS_NewSyncStreamListener(inStream, outStream, listener);

}

NS_IMETHODIMP
nsIOService::NewChannelFromNativePath(const char *nativePath, nsIFileChannel **result)
{
    nsresult rv;
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = GetProtocolHandler("file", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFileProtocolHandler> fileHandler = do_QueryInterface(handler, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIFileChannel> channel;
    rv = fileHandler->NewChannelFromNativePath(nativePath, getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;
    
    *result = channel;
    return NS_OK;
}

NS_IMETHODIMP
nsIOService::NewLoadGroup(nsILoadGroup* parent, nsILoadGroup **result)
{
    nsresult rv;
    nsILoadGroup* group;
    rv = nsLoadGroup::Create(nsnull, nsCOMTypeInfo<nsILoadGroup>::GetIID(), 
                             (void**)&group);
    if (NS_FAILED(rv)) return rv;
    *result = group;
    return NS_OK;
}


NS_IMETHODIMP
nsIOService::NewInputStreamChannel(nsIURI* uri, const char *contentType,
                                   nsIInputStream *inStr, nsIChannel **result)
{
    nsresult rv;
    nsInputStreamChannel* channel;
    rv = nsInputStreamChannel::Create(nsnull, nsCOMTypeInfo<nsIChannel>::GetIID(),
                                      (void**)&channel);
    if (NS_FAILED(rv)) return rv;
    rv = channel->Init(uri, contentType, inStr);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }
    *result = channel;
	return NS_OK;
}
    
