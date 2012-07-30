/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRDFResource.h"
#include "nsIServiceManager.h"
#include "nsIRDFDelegateFactory.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "prlog.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsIRDFService* nsRDFResource::gRDFService = nullptr;
nsrefcnt nsRDFResource::gRDFServiceRefCnt = 0;

////////////////////////////////////////////////////////////////////////////////

nsRDFResource::nsRDFResource(void)
    : mDelegates(nullptr)
{
}

nsRDFResource::~nsRDFResource(void)
{
    // Release all of the delegate objects
    while (mDelegates) {
        DelegateEntry* doomed = mDelegates;
        mDelegates = mDelegates->mNext;
        delete doomed;
    }

    if (!gRDFService)
        return;

    gRDFService->UnregisterResource(this);

    if (--gRDFServiceRefCnt == 0)
        NS_RELEASE(gRDFService);
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsRDFResource, nsIRDFResource, nsIRDFNode)

////////////////////////////////////////////////////////////////////////////////
// nsIRDFNode methods:

NS_IMETHODIMP
nsRDFResource::EqualsNode(nsIRDFNode* aNode, bool* aResult)
{
    NS_PRECONDITION(aNode != nullptr, "null ptr");
    if (! aNode)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    nsIRDFResource* resource;
    rv = aNode->QueryInterface(NS_GET_IID(nsIRDFResource), (void**)&resource);
    if (NS_SUCCEEDED(rv)) {
        *aResult = (static_cast<nsIRDFResource*>(this) == resource);
        NS_RELEASE(resource);
        return NS_OK;
    }
    else if (rv == NS_NOINTERFACE) {
        *aResult = false;
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
    NS_PRECONDITION(aURI != nullptr, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    mURI = aURI;

    if (gRDFServiceRefCnt++ == 0) {
        nsresult rv = CallGetService(kRDFServiceCID, &gRDFService);
        if (NS_FAILED(rv)) return rv;
    }

    // don't replace an existing resource with the same URI automatically
    return gRDFService->RegisterResource(this, true);
}

NS_IMETHODIMP
nsRDFResource::GetValue(char* *aURI)
{
    NS_ASSERTION(aURI, "Null out param.");
    
    *aURI = ToNewCString(mURI);

    if (!*aURI)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::GetValueUTF8(nsACString& aResult)
{
    aResult = mURI;
    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::GetValueConst(const char** aURI)
{
    *aURI = mURI.get();
    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::EqualsString(const char* aURI, bool* aResult)
{
    NS_PRECONDITION(aURI != nullptr, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult, "null ptr");

    *aResult = mURI.Equals(aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::GetDelegate(const char* aKey, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aKey != nullptr, "null ptr");
    if (! aKey)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    *aResult = nullptr;

    DelegateEntry* entry = mDelegates;
    while (entry) {
        if (entry->mKey.Equals(aKey)) {
            rv = entry->mDelegate->QueryInterface(aIID, aResult);
            return rv;
        }

        entry = entry->mNext;
    }

    // Construct a ContractID of the form "@mozilla.org/rdf/delegate/[key]/[scheme];1
    nsCAutoString contractID(NS_RDF_DELEGATEFACTORY_CONTRACTID_PREFIX);
    contractID.Append(aKey);
    contractID.Append("&scheme=");

    PRInt32 i = mURI.FindChar(':');
    contractID += StringHead(mURI, i);

    nsCOMPtr<nsIRDFDelegateFactory> delegateFactory =
             do_CreateInstance(contractID.get(), &rv);
    if (NS_FAILED(rv)) return rv;

    rv = delegateFactory->CreateDelegate(this, aKey, aIID, aResult);
    if (NS_FAILED(rv)) return rv;

    // Okay, we've successfully created a delegate. Let's remember it.
    entry = new DelegateEntry;
    if (! entry) {
        NS_RELEASE(*reinterpret_cast<nsISupports**>(aResult));
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    entry->mKey      = aKey;
    entry->mDelegate = do_QueryInterface(*reinterpret_cast<nsISupports**>(aResult), &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsRDFResource::GetDelegate(): can't QI to nsISupports!");

        delete entry;
        NS_RELEASE(*reinterpret_cast<nsISupports**>(aResult));
        return NS_ERROR_FAILURE;
    }

    entry->mNext     = mDelegates;

    mDelegates = entry;

    return NS_OK;
}

NS_IMETHODIMP
nsRDFResource::ReleaseDelegate(const char* aKey)
{
    NS_PRECONDITION(aKey != nullptr, "null ptr");
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
