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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsRDFResource.h"
#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nsIRDFDelegateFactory.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "prlog.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsIRDFService* nsRDFResource::gRDFService = nsnull;
nsrefcnt nsRDFResource::gRDFServiceRefCnt = 0;

////////////////////////////////////////////////////////////////////////////////

nsRDFResource::nsRDFResource(void)
    : mURI(nsnull), mDelegates(nsnull)
{
    NS_INIT_REFCNT();
}

nsRDFResource::~nsRDFResource(void)
{
    // Release all of the delegate objects
    while (mDelegates) {
        DelegateEntry* doomed = mDelegates;
        mDelegates = mDelegates->mNext;
        delete doomed;
    }

    gRDFService->UnregisterResource(this);

    // N.B. that we need to free the URI *after* we un-cache the resource,
    // due to the way that the resource manager is implemented.
    nsCRT::free(mURI);

    if (--gRDFServiceRefCnt == 0) {
        nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
        gRDFService = nsnull;
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsRDFResource, nsIRDFResource, nsIRDFNode)

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
    rv = aNode->QueryInterface(NS_GET_IID(nsIRDFResource), (void**)&resource);
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

    if (! (mURI = (char *)nsMemory::Alloc(nsCRT::strlen(aURI) + 1)))
        return NS_ERROR_OUT_OF_MEMORY;

    PL_strcpy(mURI, aURI);

    nsresult rv = NS_OK;
    if (gRDFServiceRefCnt++ == 0) {
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**)&gRDFService);
        if (NS_FAILED(rv)) return rv;
    }

    // don't replace an existing resource with the same URI automatically
    return gRDFService->RegisterResource(this, PR_TRUE);
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

NS_IMETHODIMP
nsRDFResource::GetDelegate(const char* aKey, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aKey != nsnull, "null ptr");
    if (! aKey)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    *aResult = nsnull;

    DelegateEntry* entry = mDelegates;
    while (entry) {
        if (entry->mKey.Equals(aKey)) {
            rv = entry->mDelegate->QueryInterface(aIID, aResult);
            return rv;
        }

        entry = entry->mNext;
    }

    // Construct a ProgID of the form "component:/rdf/delegate/[key]/[scheme]
    nsCAutoString progID(NS_RDF_DELEGATEFACTORY_PROGID_PREFIX);
    progID.Append(aKey);
    progID.Append('.');

    for (const char* p = mURI; *p && *p != ':'; ++p)
        progID.Append(*p);

    nsCOMPtr<nsIRDFDelegateFactory> delegateFactory;
    rv = nsComponentManager::CreateInstance(progID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFDelegateFactory),
                                            getter_AddRefs(delegateFactory));
    if (NS_FAILED(rv)) return rv;

    rv = delegateFactory->CreateDelegate(this, aKey, aIID, aResult);
    if (NS_FAILED(rv)) return rv;

    // Okay, we've successfully created a delegate. Let's remember it.
    entry = new DelegateEntry;
    if (! entry) {
        NS_RELEASE(*NS_REINTERPRET_CAST(nsISupports**, aResult));
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    entry->mKey      = aKey;
    entry->mDelegate = do_QueryInterface(*NS_REINTERPRET_CAST(nsISupports**, aResult));
    entry->mNext     = mDelegates;

    mDelegates = entry;

    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::ReleaseDelegate(const char* aKey)
{
    NS_PRECONDITION(aKey != nsnull, "null ptr");
    if (! aKey)
        return NS_ERROR_NULL_POINTER;

    DelegateEntry* entry = mDelegates;
    DelegateEntry** link = &mDelegates;

    while (entry) {
        if (entry->mKey.Equals(aKey)) {
            *link = entry->mNext;
            delete entry;
            return NS_OK;
        }

        link = &(entry->mNext);
        entry = entry->mNext;
    }

    NS_WARNING("nsRDFResource::ReleaseDelegate() no delegate found");
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////////////
