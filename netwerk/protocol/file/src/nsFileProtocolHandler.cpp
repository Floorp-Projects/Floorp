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

#include "nsFileChannel.h"
#include "nsFileProtocolHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"
#include "nsISupportsArray.h"
#include "nsFileSpec.h"
#include "nsAutoLock.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileProtocolHandler::nsFileProtocolHandler()
{
    NS_INIT_REFCNT();
}

nsresult
nsFileProtocolHandler::Init()
{
    return NS_OK;
}

nsFileProtocolHandler::~nsFileProtocolHandler()
{
}

NS_IMPL_ISUPPORTS2(nsFileProtocolHandler,
                   nsIFileProtocolHandler,
                   nsIProtocolHandler);

NS_METHOD
nsFileProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsFileProtocolHandler* ph = new nsFileProtocolHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsFileProtocolHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("file");
    if (*result == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for file: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                              nsIURI **result)
{
    nsresult rv;

    // file: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsIURI* url;
    if (aBaseURI) {
        rv = aBaseURI->Clone(&url);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else {
        rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                                NS_GET_IID(nsIURI),
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

NS_IMETHODIMP
nsFileProtocolHandler::NewChannel(const char* command, nsIURI* url,
                                  nsILoadGroup* aLoadGroup,
                                  nsIInterfaceRequestor* notificationCallbacks,
                                  nsLoadFlags loadAttributes,
                                  nsIURI* originalURI,
                                  PRUint32 bufferSegmentSize,
                                  PRUint32 bufferMaxSize,
                                  nsIChannel* *result)
{
    nsresult rv;
    
    nsFileChannel* channel;
    rv = nsFileChannel::Create(nsnull, NS_GET_IID(nsIFileChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(this, command, url, aLoadGroup, notificationCallbacks,
                       loadAttributes, originalURI, bufferSegmentSize, bufferMaxSize);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

NS_IMETHODIMP
nsFileProtocolHandler::NewChannelFromNativePath(const char* nativePath, 
                                                nsILoadGroup* aLoadGroup,
                                                nsIInterfaceRequestor* notificationCallbacks,
                                                nsLoadFlags loadAttributes,
                                                nsIURI* originalURI,
                                                PRUint32 bufferSegmentSize,
                                                PRUint32 bufferMaxSize,
                                                nsIFileChannel* *result)
{
    nsresult rv;
    nsFileSpec spec(nativePath);
    nsFileURL fileURL(spec);
    const char* urlStr = fileURL.GetURLString();
    nsIURI* uri;

    rv = NewURI(urlStr, nsnull, &uri);
    if (NS_FAILED(rv)) return rv;

    rv = NewChannel("load",  // XXX what should this be?
                    uri, 
                    aLoadGroup,
                    notificationCallbacks,
                    loadAttributes,
                    originalURI,
                    bufferSegmentSize,
                    bufferMaxSize,
                    (nsIChannel**)result);
    NS_RELEASE(uri);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
