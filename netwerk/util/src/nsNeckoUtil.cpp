/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsNeckoUtil.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIAllocator.h"
#include "nsCOMPtr.h"
#include "nsIHTTPProtocolHandler.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kLoadGroupCID, NS_LOADGROUP_CID);

NECKO_EXPORT(nsresult)
NS_NewURI(nsIURI* *result, const char* spec, nsIURI* baseURI)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = serv->NewURI(spec, baseURI, result);
    return rv;
}

NECKO_EXPORT(nsresult)
NS_NewURI(nsIURI* *result, const nsString& spec, nsIURI* baseURI)
{
    // XXX if the string is unicode, GetBuffer() returns null. 
    // XXX we need a strategy to deal w/ unicode specs (if there should
    // XXX even be such a thing)
    char* specStr = spec.ToNewCString(); // this forces a single byte char*
    if (specStr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = NS_NewURI(result, specStr, baseURI);
    nsAllocator::Free(specStr);
    return rv;
}

NECKO_EXPORT(nsresult)
NS_OpenURI(nsIChannel* *result, nsIURI* uri,
           nsILoadGroup *aGroup,
           nsIInterfaceRequestor *notificationCallbacks, 
           nsLoadFlags loadAttributes)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = serv->NewChannelFromURI("load", uri, aGroup, notificationCallbacks,
                                 loadAttributes, nsnull, &channel);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

NECKO_EXPORT(nsresult)
NS_OpenURI(nsIInputStream* *result, nsIURI* uri, 
           nsILoadGroup *aGroup,
           nsIInterfaceRequestor *notificationCallbacks, 
           nsLoadFlags loadAttributes)
{
    nsresult rv;
    nsIChannel* channel;

    rv = NS_OpenURI(&channel, uri, aGroup, notificationCallbacks, loadAttributes);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* inStr;
    rv = channel->OpenInputStream(0, -1, &inStr);
    NS_RELEASE(channel);
    if (NS_FAILED(rv)) return rv;

    *result = inStr;
    return rv;
}

NECKO_EXPORT(nsresult)
NS_OpenURI(nsIStreamListener* aConsumer, nsISupports* context, nsIURI* uri, 
           nsILoadGroup *aGroup,
           nsIInterfaceRequestor *notificationCallbacks, 
           nsLoadFlags loadAttributes)
{
    nsresult rv;
    nsIChannel* channel;

    rv = NS_OpenURI(&channel, uri, aGroup, notificationCallbacks, loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = channel->AsyncRead(0, -1, context, aConsumer);
    NS_RELEASE(channel);
    return rv;
}

// XXX copied from nsIOService.cpp (for now):
static nsresult
GetScheme(const char* inURI, char* *scheme)
{
    // search for something up to a colon, and call it the scheme
    NS_ASSERTION(inURI, "null pointer");
    if (!inURI) return NS_ERROR_NULL_POINTER;
    char c;
    const char* URI = inURI;
    PRUint8 length = 0;
    // skip leading white space
    while (nsString::IsSpace(*URI))
        URI++;
    while ((c = *URI++) != '\0') {
        if (c == ':') {
            char* newScheme = (char *)nsAllocator::Alloc(length+1);
            if (newScheme == nsnull)
                return NS_ERROR_OUT_OF_MEMORY;

            nsCRT::memcpy(newScheme, inURI, length);
            newScheme[length] = '\0';
            *scheme = newScheme;
            return NS_OK;
        }
        else if (nsString::IsAlpha(c)) {
            length++;
        }
        else 
            break;
    }
    return NS_ERROR_MALFORMED_URI;
}

NECKO_EXPORT(nsresult)
NS_MakeAbsoluteURI(const char* aSpec, nsIURI* aBaseURI, char* *result)
{
    nsresult rv;
    NS_ASSERTION(aBaseURI, "It doesn't make sense to not supply a base URI");
 
    if (aSpec == nsnull)
        return aBaseURI->GetSpec(result);
     
    char* scheme;
    rv = GetScheme(aSpec, &scheme);
    if (NS_SUCCEEDED(rv)) {
        nsAllocator::Free(scheme);
        // if aSpec has a scheme, then it's already absolute
        *result = nsCRT::strdup(aSpec);
        return (*result == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
     
    return aBaseURI->Resolve(aSpec, result);
}

NECKO_EXPORT(nsresult)
NS_MakeAbsoluteURI(const nsString& spec, nsIURI* baseURI, nsString& result)
{
    char* resultStr;
    char* specStr = spec.ToNewCString();
    if (!specStr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = NS_MakeAbsoluteURI(specStr, baseURI, &resultStr);
    nsAllocator::Free(specStr);
    if (NS_FAILED(rv)) return rv;

    result = resultStr;
    nsAllocator::Free(resultStr);
    return rv;
}

NECKO_EXPORT(nsresult)
NS_NewLoadGroup(nsISupports* outer, nsIStreamObserver* observer,
                nsILoadGroup* parent, nsILoadGroup* *result)
{
    nsresult rv;
    nsILoadGroup* group;
    rv = nsComponentManager::CreateInstance(kLoadGroupCID, outer,
                                            NS_GET_IID(nsILoadGroup), 
                                            (void**)&group);
    if (NS_FAILED(rv)) return rv;

    rv = group->Init(observer, parent);
    if (NS_FAILED(rv)) {
        NS_RELEASE(group);
        return rv;
    }
    *result = group;    
    return rv;
}

NECKO_EXPORT(nsresult)
NS_NewPostDataStream(PRBool isFile, const char *data, PRUint32 encodeFlags,
                     nsIInputStream **result)
{
    nsresult rv;
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = serv->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTTPProtocolHandler> http = do_QueryInterface(handler, &rv);
    if (NS_FAILED(rv)) return rv;
    
    return http->NewPostDataStream(isFile, data, encodeFlags, result);
}

////////////////////////////////////////////////////////////////////////////////
