/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsKeywordProtocolHandler.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

////////////////////////////////////////////////////////////////////////////////

nsKeywordProtocolHandler::nsKeywordProtocolHandler() {
}

nsresult
nsKeywordProtocolHandler::Init() {
    nsresult rv;
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString url;
    rv = prefs->GetCharPref("keyword.URL", getter_Copies(url));
    // if we can't find a keyword.URL keywords won't work.
    if (NS_FAILED(rv) || !url || !*url) return NS_ERROR_FAILURE;

    mKeywordURL.Assign(url);
    return NS_OK;
}

nsKeywordProtocolHandler::~nsKeywordProtocolHandler() {
}

NS_IMPL_ISUPPORTS1(nsKeywordProtocolHandler, nsIProtocolHandler)

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
nsKeywordProtocolHandler::GetScheme(nsACString &result) {
    result = "keyword";
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::GetDefaultPort(PRInt32 *result) {
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::GetProtocolFlags(PRUint32 *result) {
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

// digests a spec _without_ the preceeding "keyword:" scheme.
static char *
MangleKeywordIntoHTTPURL(const char *aSpec, const char *aHTTPURL) {
    char * unescaped = nsCRT::strdup(aSpec);
    if(unescaped == nsnull) 
       return nsnull;

    // XXX this doesn't play nicely w/ i18n URLs
    nsUnescape(unescaped);

    // build up a request to the keyword server.
    nsCAutoString query;

    // pull out the "go" action word, or '?', if any
    char one = unescaped[0], two = unescaped[1];
    if (one == '?') {                            // "?blah"
        query = unescaped+1;
    } else if ( (one == 'g' || one == 'G')       //
                            &&                   //
                (two == 'o' || two == 'O')       // "g[G]o[O] blah"
                            &&                   //
                     (unescaped[2] == ' ') ) {      //

        query = unescaped+3;
    } else {
        query = unescaped;
    }

    nsMemory::Free(unescaped);

    query.Trim(" "); // pull leading/trailing spaces.

    // encode
    char * encQuery = nsEscape(query.get(), url_Path);
    if (!encQuery) return nsnull;
    query = encQuery;
    nsMemory::Free(encQuery);

    // prepend the query with the keyword url
    // XXX this url should come from somewhere else
    query.Insert(aHTTPURL, 0);

    return ToNewCString(query);
}

// digests a spec of the form "keyword:blah"
NS_IMETHODIMP
nsKeywordProtocolHandler::NewURI(const nsACString &aSpec,
                                 const char *aCharset, // ignore charset info
                                 nsIURI *aBaseURI,
                                 nsIURI **result)
{
    nsIURI* uri;
    nsresult rv = CallCreateInstance(kSimpleURICID, &uri);
    if (NS_FAILED(rv)) return rv;

    rv = uri->SetSpec(aSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(uri);
        return rv;
    }

    *result = uri;
    return rv;
}

NS_IMETHODIMP
nsKeywordProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;

    NS_ASSERTION(!mKeywordURL.IsEmpty(), "someone's trying to use the keyword handler even though it hasn't been init'd");

    nsCAutoString path;
    rv = uri->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    char *httpSpec = MangleKeywordIntoHTTPURL(path.get(), mKeywordURL.get());
    if (!httpSpec) return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIIOService> serv(do_GetIOService(&rv));
    if (NS_FAILED(rv)) return rv;

    // now we have an HTTP url, give the user an HTTP channel
    rv = serv->NewChannel(nsDependentCString(httpSpec), nsnull, nsnull, result);
    nsMemory::Free(httpSpec);
    return rv;

}

NS_IMETHODIMP 
nsKeywordProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
