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

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "nsIURI.h"
#include "netCore.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsString.h"

#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIAllocator.h"
#include "nsCOMPtr.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsIUnicharStreamLoader.h"

inline nsresult
NS_NewURI(nsIURI* *result, const char* spec, nsIURI* baseURI = nsnull)
{
    nsresult rv;
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    rv = serv->NewURI(spec, baseURI, result);
    return rv;
}

inline nsresult
NS_NewURI(nsIURI* *result, const nsString& spec, nsIURI* baseURI = nsnull)
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

inline nsresult
NS_OpenURI(nsIChannel* *result, nsIURI* uri, nsILoadGroup *aGroup,
           nsIInterfaceRequestor *capabilities = nsnull)
{
    nsresult rv;
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = serv->NewChannelFromURI("load", uri, aGroup, capabilities, 
                                 nsnull, &channel);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    return rv;
}

inline nsresult
NS_OpenURI(nsIInputStream* *result, nsIURI* uri)
{
    nsresult rv;
    nsIChannel* channel;

    rv = NS_OpenURI(&channel, uri, nsnull);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* inStr;
    rv = channel->OpenInputStream(0, -1, &inStr);
    NS_RELEASE(channel);
    if (NS_FAILED(rv)) return rv;

    *result = inStr;
    return rv;
}

inline nsresult
NS_OpenURI(nsIStreamListener* aConsumer, nsISupports* context, nsIURI* uri, 
           nsILoadGroup *aGroup)
{
    nsresult rv;
    nsIChannel* channel;

    rv = NS_OpenURI(&channel, uri, aGroup);
    if (NS_FAILED(rv)) return rv;

    rv = channel->AsyncRead(0, -1, context, aConsumer);
    NS_RELEASE(channel);
    return rv;
}

inline nsresult
NS_MakeAbsoluteURI(const char* spec, nsIURI* baseURI, char* *result)
{
    return baseURI->Resolve(spec, result);
}

inline nsresult
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

inline nsresult
NS_NewPostDataStream(PRBool isFile, const char *data, PRUint32 encodeFlags,
                     nsIInputStream **result)
{
    nsresult rv;
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = serv->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTTPProtocolHandler> http = do_QueryInterface(handler, &rv);
    if (NS_FAILED(rv)) return rv;
    
    return http->NewPostDataStream(isFile, data, encodeFlags, result);
}

inline nsresult
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader* *result,
                          nsIURI* uri,
                          nsILoadGroup* loadGroup,
                          nsIUnicharStreamLoaderObserver* observer)
{
    nsresult rv;
    nsCOMPtr<nsIUnicharStreamLoader> loader;
    static NS_DEFINE_CID(kUnicharStreamLoaderCID, NS_UNICHARSTREAMLOADER_CID);
    rv = nsComponentManager::CreateInstance(kUnicharStreamLoaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIUnicharStreamLoader),
                                            getter_AddRefs(loader));
    if (NS_FAILED(rv)) return rv;
    rv = loader->Init(uri, loadGroup, observer);
    if (NS_FAILED(rv)) return rv;
    *result = loader;
    NS_ADDREF(*result);
    return rv;
}

#endif // nsNetUtil_h__
