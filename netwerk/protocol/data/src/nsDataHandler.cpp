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

#include "nspr.h"
#include "nsDataChannel.h"
#include "nsDataHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"

static NS_DEFINE_CID(kSimpleURICID,            NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsDataHandler::nsDataHandler() {
    NS_INIT_REFCNT();
}

nsDataHandler::~nsDataHandler() {
}

NS_IMPL_ISUPPORTS1(nsDataHandler, nsIProtocolHandler)

NS_METHOD
nsDataHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult) {

    nsDataHandler* ph = new nsDataHandler();
    if (ph == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->QueryInterface(aIID, aResult);
    NS_RELEASE(ph);
    return rv;
}
    
////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsDataHandler::GetScheme(char* *result) {
    *result = nsCRT::strdup("data");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::GetDefaultPort(PRInt32 *result) {
    // no ports for data protocol
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsDataHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                             nsIURI **result) {
    nsresult rv;

    // no concept of a relative data url
    NS_ASSERTION(!aBaseURI, "base url passed into data protocol handler");

    nsCAutoString spec(aSpec);

    nsIURI* url;
    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;
    rv = url->SetSpec(spec.GetBuffer());
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsDataHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    nsresult rv;
    
    nsDataChannel* channel;
    rv = nsDataChannel::Create(nsnull, NS_GET_IID(nsIDataChannel), (void**)&channel);
    if (NS_FAILED(rv)) return rv;

    rv = channel->Init(url);
    if (NS_FAILED(rv)) {
        NS_RELEASE(channel);
        return rv;
    }

    *result = channel;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
