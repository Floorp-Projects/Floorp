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
#include "plstr.h"
#include "prprf.h"
#include "prtime.h"
#include "rdfutil.h"

#include "rdf.h"
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFServiceIID,  NS_IRDFSERVICE_IID);

static NS_DEFINE_CID(kRDFServiceCID,   NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

// XXX This'll permanently leak
static nsIRDFService* gRDFService = nsnull;

static nsIRDFResource* kRDF_instanceOf = nsnull;
static nsIRDFResource* kRDF_Bag        = nsnull;
static nsIRDFResource* kRDF_Seq        = nsnull;
static nsIRDFResource* kRDF_Alt        = nsnull;
static nsIRDFResource* kRDF_nextVal    = nsnull;

static nsresult
rdf_EnsureRDFService(void)
{
    if (gRDFService)
        return NS_OK;

    nsresult rv;

    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      kIRDFServiceIID,
                                      (nsISupports**) &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) return rv;

    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "instanceOf", &kRDF_instanceOf);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Bag",        &kRDF_Bag);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Seq",        &kRDF_Seq);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "Alt",        &kRDF_Alt);
    rv = gRDFService->GetResource(RDF_NAMESPACE_URI "nextVal",    &kRDF_nextVal);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

PRBool
rdf_IsA(nsIRDFDataSource* aDataSource, nsIRDFResource* aResource, nsIRDFResource* aType)
{
    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return PR_FALSE;

    PRBool result;
    rv = aDataSource->HasAssertion(aResource, kRDF_instanceOf, aType, PR_TRUE, &result);
    if (NS_FAILED(rv)) return PR_FALSE;

    return result;
}


nsresult
rdf_CreateAnonymousResource(const nsString& aContextURI, nsIRDFResource** aResult)
{
static PRUint32 gCounter = 0;

    nsresult rv;
    rv = rdf_EnsureRDFService();
    if (NS_FAILED(rv)) return rv;

    if (! gCounter) {
        // Start it at a semi-unique value, just to minimize the
        // chance that we get into a situation where
        //
        // 1. An anonymous resource gets serialized out in a graph
        // 2. Reboot
        // 3. The same anonymous resource gets requested, and refers
        //    to something completely different.
        // 4. The serialization is read back in.
        LL_L2UI(gCounter, PR_Now());
    }

    do {
        nsAutoString s(aContextURI);
        s.Append("#$");
        s.Append(++gCounter, 16);

        nsIRDFResource* resource;
        if (NS_FAILED(rv = gRDFService->GetUnicodeResource(s.GetUnicode(), &resource)))
            return rv;

        // XXX an ugly but effective way to make sure that this
        // resource is really unique in the world.
        nsrefcnt refcnt = resource->AddRef();
        resource->Release();

        if (refcnt == 2) {
            *aResult = resource;
            break;
        }
    } while (1);

    return NS_OK;
}

PRBool
rdf_IsAnonymousResource(const nsString& aContextURI, nsIRDFResource* aResource)
{
    nsresult rv;
    nsXPIDLCString s;
    if (NS_FAILED(rv = aResource->GetValue( getter_Copies(s) ))) {
        NS_ASSERTION(PR_FALSE, "unable to get resource URI");
        return PR_FALSE;
    }

    nsAutoString uri((const char*) s);

    // Make sure that they have the same context (prefix)
    if (uri.Find(aContextURI) != 0)
        return PR_FALSE;

    uri.Cut(0, aContextURI.Length());

    // Anonymous resources look like the regexp "\$[0-9]+"
    if (uri.CharAt(0) != '#' || uri.CharAt(1) != '$')
        return PR_FALSE;

    for (PRInt32 i = uri.Length() - 1; i >= 1; --i) {
        if (uri.CharAt(i) < '0' || uri.CharAt(i) > '9')
            return PR_FALSE;
    }

    return PR_TRUE;
}


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
    delete[] uriStr;

    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString absUrlStr;
    char *urlSpec = aURI.ToNewCString();
    if (! urlSpec)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = service->MakeAbsolute(urlSpec, baseUri, getter_Copies(absUrlStr));
    delete[] urlSpec;

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
