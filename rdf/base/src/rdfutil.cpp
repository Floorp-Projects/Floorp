/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  Implementations for a bunch of useful RDF utility routines.  Many of
  these will eventually be exported outside of RDF.DLL via the
  nsIRDFService interface.

  TO DO

  1) Make this so that it doesn't permanently leak the RDF service
     object.

  2) Make container functions thread-safe. They currently don't ensure
     that the RDF:nextVal property is maintained safely.

 */

#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prtime.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

nsresult
rdf_MakeRelativeRef(const nsString& aBaseURI, nsString& aURI)
{
    // This implementation is extremely simple: e.g., it can't compute
    // relative paths, or anything fancy like that. If the context URI
    // is not a prefix of the URI in question, we'll just bail.
    if (aURI.Find(aBaseURI) != 0)
        return NS_OK;

    // Otherwise, pare down the target URI, removing the context URI.
    aURI.Cut(0, aBaseURI.Length());

    if (aURI.First() == '/')
        aURI.Cut(0, 1);

    return NS_OK;
}


nsresult
rdf_MakeRelativeName(const nsString& aBaseURI, nsString& aURI)
{
    nsresult rv;

    rv = rdf_MakeRelativeRef(aBaseURI, aURI);
    if (NS_FAILED(rv)) return rv;
    
    if (aURI.First() == '#')
        aURI.Cut(0, 1);

    return NS_OK;
}


nsresult
rdf_MakeAbsoluteURI(const nsString& aBaseURI, nsString& aURI)
{
    nsresult rv;
    nsAutoString result;

#ifndef NECKO
    rv = NS_MakeAbsoluteURL(nsnull, aBaseURI, aURI, result);
#else
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURI> baseUri;

    char *uriStr = aBaseURI.ToNewCString();
    if (! uriStr)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = service->NewURI(uriStr, nsnull, getter_AddRefs(baseUri));
    nsCRT::free(uriStr);

    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString absUrlStr;
    char *urlSpec = aURI.ToNewCString();
    if (! urlSpec)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = service->MakeAbsolute(urlSpec, baseUri, getter_Copies(absUrlStr));
    nsCRT::free(urlSpec);

    result = (const char*) absUrlStr;
#endif // NECKO
    if (NS_SUCCEEDED(rv)) {
        aURI = result;
    }
    else {
        // There are some ugly URIs (e.g., "NC:Foo") that netlib can't
        // parse. If NS_MakeAbsoluteURL fails, then just punt and
        // assume that aURI was already absolute.
    }

    return NS_OK;
}


nsresult
rdf_MakeAbsoluteURI(nsIURI* aURL, nsString& aURI)
{
    nsresult rv;
    nsAutoString result;

#ifndef NECKO
    rv = NS_MakeAbsoluteURL(aURL, nsAutoString(""), aURI, result);
#else
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIURI *baseUri = nsnull;
    rv = aURL->QueryInterface(nsIURI::GetIID(), (void**)&baseUri);
    if (NS_FAILED(rv)) return rv;

    char *absUrlStr = nsnull;
    rv = service->MakeAbsolute(nsCAutoString(aURI), baseUri, &absUrlStr);
    NS_RELEASE(baseUri);
    result = absUrlStr;
    nsCRT::free(absUrlStr);
#endif // NECKO
    if (NS_SUCCEEDED(rv)) {
        aURI = result;
    }
    else {
        // There are some ugly URIs (e.g., "NC:Foo") that netlib can't
        // parse. If NS_MakeAbsoluteURL fails, then just punt and
        // assume that aURI was already absolute.
    }

    return NS_OK;
}
