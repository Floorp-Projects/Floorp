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

#include "nsKeywordProtocolHandler.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kStdURICID,     NS_STANDARDURL_CID);
static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsKeywordProtocolHandler::nsKeywordProtocolHandler() {
    NS_INIT_REFCNT();
}

nsresult
nsKeywordProtocolHandler::Init() {
    return NS_OK;
}

nsKeywordProtocolHandler::~nsKeywordProtocolHandler() {
}

NS_IMPL_ISUPPORTS(nsKeywordProtocolHandler, NS_GET_IID(nsIProtocolHandler));

NS_METHOD
nsKeywordProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsKeywordProtocolHandler* ph = new nsKeywordProtocolHandler();
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
nsKeywordProtocolHandler::GetScheme(char* *result) {
    *result = nsCRT::strdup("keyword");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::GetDefaultPort(PRInt32 *result) {
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::MakeAbsolute(const char* aSpec,
                                     nsIURI* aBaseURI,
                                     char* *result) {
    // presumably, there's no such thing as a relative Keyword: URI,
    // so just copy the input spec
    char* dup = nsCRT::strdup(aSpec);
    if (!dup) return NS_ERROR_OUT_OF_MEMORY;
    *result = dup;
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                               nsIURI **result) {
    nsresult rv;

    // build up a request to the keyword server.
    nsCAutoString query;

    // pull out the "go" action word, or '?', if any
    char one = aSpec[10], two = aSpec[11];
    if ( ((one == '?') && (two == ' ')) ) {      // "? blah"
        query = aSpec+11;
    } else if ( (one == 'g' || one == 'G')       //
                            &&                   //
                (two == 'o' || two == 'O')       // "g[G]o[O] blah"
                            &&                   //
                     (aSpec[12] == ' ') ) {      //

        query = aSpec+11;
    } else {
        // we need to skip over the original "keyword:" scheme in the string.
        query = aSpec+8;
    }

    query.Trim(" "); // pull leading/trailing spaces.

    // encode
    char * encQuery = nsEscape(query.GetBuffer(), url_Path);
    if (!encQuery) return NS_ERROR_OUT_OF_MEMORY;
    query = encQuery;
    nsAllocator::Free(encQuery);

    // prepend the query with the keyword url
    // XXX this url should come from somewhere else
    query.Insert("http://keyword.netscape.com/keyword/", 0);

    nsIURI* url;

    rv = nsComponentManager::CreateInstance(kStdURICID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            (void**)&url);
    if (NS_FAILED(rv)) return rv;
    rv = url->SetSpec(query.GetBuffer());

    if (NS_FAILED(rv)) {
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                   nsILoadGroup *aGroup,
                                   nsIEventSinkGetter* eventSinkGetter,
                                   nsIChannel* *result) {
    // the keyword handler simply munges the spec during
    // uri creation, and creates an HTTP url.
    NS_ASSERTION(0, "someone's trying to create a new keyword channel");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
