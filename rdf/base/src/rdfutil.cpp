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
#include "nsRDFCID.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "prprf.h"
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

PR_IMPLEMENT(PRBool)
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

    do {
        nsAutoString s(aContextURI);
        s.Append("#$");
        s.Append(++gCounter, 10);

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


PR_EXTERN(nsresult)
rdf_PossiblyMakeRelative(const nsString& aContextURI, nsString& aURI)
{
    // This implementation is extremely simple: no ".." or anything
    // fancy like that. If the context URI is not a prefix of the URI
    // in question, we'll just bail.
    if (aURI.Find(aContextURI) != 0)
        return NS_OK;

    // Otherwise, pare down the target URI, removing the context URI.
    aURI.Cut(0, aContextURI.Length());

    if (aURI.First() == '#' || aURI.First() == '/')
        aURI.Cut(0, 1);

    return NS_OK;
}


PR_EXTERN(nsresult)
rdf_PossiblyMakeAbsolute(const nsString& aContextURI, nsString& aURI)
{
    PRInt32 index = aURI.Find(':');
    if (index > 0 && index < 25 /* XXX */)
        return NS_OK;

    PRUnichar last  = aContextURI.Last();
    PRUnichar first = aURI.First();

    nsAutoString result(aContextURI);
    if (last == '#' || last == '/') {
        if (first == '#') {
            result.Truncate(result.Length() - 2);
        }
        result.Append(aURI);
    }
    else if (first == '#') {
        result.Append(aURI);
    }
    else {
        result.Append('#');
        result.Append(aURI);
    }

    aURI = result;
    return NS_OK;
}


