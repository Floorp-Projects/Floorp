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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsIIOService.h"
#include "nsIURL.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "prtime.h"
#include "rdfutil.h"
#include "nsNetUtil.h"

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

    nsCOMPtr<nsIURI> base;
    rv = NS_NewURI(getter_AddRefs(base), aBaseURI);
    if (NS_FAILED(rv)) return rv;

    rv = NS_MakeAbsoluteURI(aURI, base, result);
    if (NS_FAILED(rv)) return rv;

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

    rv = NS_MakeAbsoluteURI(aURI, aURL, result);

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
rdf_MakeAbsoluteURI(nsIURI* aURL, nsCString& aURI)
{
    nsresult rv;
    nsXPIDLCString result;

    rv = NS_MakeAbsoluteURI(aURI.GetBuffer(), aURL, getter_Copies(result));

    if (NS_SUCCEEDED(rv)) {
        aURI.Assign(result);
    }
    else {
        // There are some ugly URIs (e.g., "NC:Foo") that netlib can't
        // parse. If NS_MakeAbsoluteURL fails, then just punt and
        // assume that aURI was already absolute.
    }

    return NS_OK;
}
