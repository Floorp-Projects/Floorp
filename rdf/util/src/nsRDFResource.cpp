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

#include "nsRDFResource.h"
#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "prlog.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

nsRDFResource::nsRDFResource(void)
    : mURI(nsnull)
{
    NS_INIT_REFCNT();
}

nsRDFResource::~nsRDFResource(void)
{
    nsIRDFService* rdfService = nsnull;
    nsresult rv = NS_OK;
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &rdfService);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");

    if (NS_SUCCEEDED(rv) && rdfService != nsnull) {
        rdfService->UnregisterResource(this);
        nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
    }

    // N.B. that we need to free the URI *after* we un-cache the resource,
    // due to the way that the resource manager is implemented.
    delete[] mURI;
}

NS_IMPL_ADDREF(nsRDFResource)
NS_IMPL_RELEASE(nsRDFResource)

NS_IMETHODIMP
nsRDFResource::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = nsnull;
    if (aIID.Equals(nsIRDFResource::GetIID()) ||
        aIID.Equals(nsIRDFNode::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFResource*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRDFNode methods:

NS_IMETHODIMP
nsRDFResource::EqualsNode(nsIRDFNode* aNode, PRBool* aResult)
{
    NS_PRECONDITION(aNode != nsnull, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsIRDFResource* resource;
    rv = aNode->QueryInterface(nsIRDFResource::GetIID(), (void**)&resource);
    if (NS_SUCCEEDED(rv)) {
        *aResult = (NS_STATIC_CAST(nsIRDFResource*, this) == resource);
        NS_RELEASE(resource);
        return NS_OK;
    }
    else if (rv == NS_NOINTERFACE) {
        *aResult = PR_FALSE;
        return NS_OK;
    }
    else {
        return rv;
    }
}

////////////////////////////////////////////////////////////////////////////////
// nsIRDFResource methods:

NS_IMETHODIMP
nsRDFResource::Init(const char* aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    if (! (mURI = new char[PL_strlen(aURI) + 1]))
        return NS_ERROR_OUT_OF_MEMORY;

    PL_strcpy(mURI, aURI);

    nsIRDFService* rdfService = nsnull;
    nsresult rv = NS_OK;
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                                   nsIRDFService::GetIID(),
                                                   (nsISupports**) &rdfService);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");

    if (!NS_SUCCEEDED(rv)) {
	return rv;
    }
    if (rdfService == nsnull) {
        return NS_ERROR_NULL_POINTER;
    }

    // don't replace an existing resource with the same URI automatically
    rv = rdfService->RegisterResource(this, PR_TRUE);
    nsServiceManager::ReleaseService(kRDFServiceCID, rdfService);
    return rv;
}

NS_IMETHODIMP
nsRDFResource::GetValue(char* *aURI)
{
    if (!aURI)
        return NS_ERROR_NULL_POINTER;
    
    if ((*aURI = nsXPIDLCString::Copy(mURI)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    else
        return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::GetValueConst(const char** aURI)
{
    *aURI = mURI;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::EqualsString(const char* aURI, PRBool* aResult)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = (nsCRT::strcmp(aURI, mURI) == 0);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
