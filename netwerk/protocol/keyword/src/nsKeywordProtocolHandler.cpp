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

static NS_DEFINE_CID(kSimpleURICID,     NS_SIMPLEURI_CID);
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

// digests a spec _without_ the preceeding "keyword:" scheme.
static char *
MangleKeywordIntoHTTPURL(const char *aSpec) {
    // build up a request to the keyword server.
    nsCAutoString query;

    // pull out the "go" action word, or '?', if any
    char one = aSpec[0], two = aSpec[1];
    if ( ((one == '?') && (two == ' ')) ) {      // "? blah"
        query = aSpec+2;
    } else if ( (one == 'g' || one == 'G')       //
                            &&                   //
                (two == 'o' || two == 'O')       // "g[G]o[O] blah"
                            &&                   //
                     (aSpec[12] == ' ') ) {      //

        query = aSpec+3;
    } else {
        query = aSpec;
    }

    query.Trim(" "); // pull leading/trailing spaces.

    // encode
    char * encQuery = nsEscape(query.GetBuffer(), url_Path);
    if (!encQuery) return nsnull;
    query = encQuery;
    nsAllocator::Free(encQuery);

    // prepend the query with the keyword url
    // XXX this url should come from somewhere else
    query.Insert("http://keyword.netscape.com/keyword/", 0);

    return query.ToNewCString();
}

// digests a spec of the form "keyword:blah"
NS_IMETHODIMP
nsKeywordProtocolHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                               nsIURI **result) {
    nsresult rv;
    nsIURI* uri;

    rv = nsComponentManager::CreateInstance(kSimpleURICID, nsnull, NS_GET_IID(nsIURI), (void**)&uri);
    if (NS_FAILED(rv)) return rv;
    rv = uri->SetSpec((char*)aSpec);
    if (NS_FAILED(rv)) return rv;

    *result = uri;
    return rv;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::NewChannel(const char* verb, nsIURI* uri,
                                   nsILoadGroup *aGroup,
                                   nsIEventSinkGetter* eventSinkGetter,
                                   nsIURI* aOriginalURI,
                                   nsIChannel* *result) {
    nsresult rv;

    char *path = nsnull;
    rv = uri->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    char *httpSpec = MangleKeywordIntoHTTPURL(path);
    nsAllocator::Free(path);
    if (!httpSpec) return NS_ERROR_OUT_OF_MEMORY;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // now we have an HTTP url, give the user an HTTP channel
    rv = serv->NewChannel(verb, httpSpec, nsnull, aGroup, eventSinkGetter, aOriginalURI, result);
    nsAllocator::Free(httpSpec);
    return rv;

}

////////////////////////////////////////////////////////////////////////////////
