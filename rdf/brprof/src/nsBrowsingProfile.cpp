/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIBrowsingProfile.h"
#include "nsIRDFObserver.h"
#include "nsCRT.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFCursor.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Page);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, CategoryID);

#if 0
static PRBool
peq(nsIRDFResource* r1, nsIRDFResource* r2)
{
    PRBool eq;
    return NS_SUCCEEDED(r1->EqualsResource(r2, &eq)) && eq;
}
#else
#define peq(r1, r2)     ((r1) == (r2))
#endif

////////////////////////////////////////////////////////////////////////////////

class nsBrowsingProfile : public nsIBrowsingProfile, public nsIRDFObserver {
public:
    NS_DECL_ISUPPORTS

    // nsIBrowsingProfile methods:
    NS_IMETHOD Init(const char* userProfileName);
    NS_IMETHOD GetVector(nsBrowsingProfileVector& result);
    NS_IMETHOD SetVector(nsBrowsingProfileVector& value);
    NS_IMETHOD GetCookieString(char buf[kBrowsingProfileCookieSize]);
    NS_IMETHOD SetCookieString(char buf[kBrowsingProfileCookieSize]);
    NS_IMETHOD GetDescription(char* *htmlResult);
    NS_IMETHOD CountPageVisit(const char* url);

    // nsIRDFObserver methods:
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object);
    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

    // nsBrowsingProfile methods:
    nsBrowsingProfile();
    virtual ~nsBrowsingProfile();

    nsresult RecordHit(const char* categoryURL, PRInt32 id);

    static PRUint32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFDataSource* gCategoryDB; 
    static nsIRDFResource* gPageProperty;
    static nsIRDFResource* gChildProperty;
    static nsIRDFResource* gCategoryIDProperty;

protected:
    const char* mUserProfileName;
    nsBrowsingProfileVector mVector;
};

PRUint32 nsBrowsingProfile::gRefCnt = 0;
nsIRDFService* nsBrowsingProfile::gRDFService = nsnull;
nsIRDFDataSource* nsBrowsingProfile::gCategoryDB = nsnull; 
nsIRDFResource* nsBrowsingProfile::gPageProperty = nsnull;
nsIRDFResource* nsBrowsingProfile::gChildProperty = nsnull;
nsIRDFResource* nsBrowsingProfile::gCategoryIDProperty = nsnull;

////////////////////////////////////////////////////////////////////////////////

nsBrowsingProfile::nsBrowsingProfile()
{
    nsCRT::zero(&mVector, sizeof(nsBrowsingProfileVector));
    gRefCnt++;
}

NS_IMETHODIMP
nsBrowsingProfile::Init(const char* userProfileName)
{
    nsresult rv = NS_OK;

    mUserProfileName = userProfileName;

    if (gRefCnt == 1) {
        NS_ASSERTION(gPageProperty == nsnull, "out of sync");

        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsIRDFService::GetIID(),
                                          (nsISupports**)&gRDFService);
        if (NS_FAILED(rv)) return rv;

        rv = gRDFService->GetDataSource("rdf:categoryDB", &gCategoryDB);
        if (NS_FAILED(rv)) return rv;

        // get all the properties we'll need:
        rv = gRDFService->GetResource(kURINC_Page, &gPageProperty);
        if (NS_FAILED(rv)) return rv;
        rv = gRDFService->GetResource(kURINC_Page, &gChildProperty);
        if (NS_FAILED(rv)) return rv;
        rv = gRDFService->GetResource(kURINC_Page, &gCategoryIDProperty);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

nsBrowsingProfile::~nsBrowsingProfile()
{
    --gRefCnt;
    if (gRefCnt == 0) {
        nsresult rv;

        // release all the properties:
        if (gCategoryIDProperty) {
            rv = gRDFService->UnregisterResource(gCategoryIDProperty);
            NS_ASSERTION(NS_SUCCEEDED(rv), "UnregisterResource failed");
            gCategoryIDProperty = nsnull;
        }
        if (gChildProperty) {
            rv = gRDFService->UnregisterResource(gChildProperty);
            NS_ASSERTION(NS_SUCCEEDED(rv), "UnregisterResource failed");
            gChildProperty = nsnull;
        }
        if (gPageProperty) {
            rv = gRDFService->UnregisterResource(gPageProperty);
            NS_ASSERTION(NS_SUCCEEDED(rv), "UnregisterResource failed");
            gPageProperty = nsnull;
        }

        if (gCategoryDB) {
            NS_RELEASE(gCategoryDB);
            gCategoryDB = nsnull;
        }

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
    }
}

NS_IMPL_ADDREF(nsBrowsingProfile)
NS_IMPL_RELEASE(nsBrowsingProfile)

NS_IMETHODIMP
nsBrowsingProfile::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIBrowsingProfile::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIBrowsingProfile*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    if (aIID.Equals(nsIRDFObserver::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIRDFObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// nsIBrowsingProfile methods:

NS_IMETHODIMP
nsBrowsingProfile::GetVector(nsBrowsingProfileVector& result)
{
    result = mVector;   // copy
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::SetVector(nsBrowsingProfileVector& value)
{
    mVector = value;   // copy
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::GetCookieString(char buf[kBrowsingProfileCookieSize])
{
    // XXX translate mVector to hex
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::SetCookieString(char buf[kBrowsingProfileCookieSize])
{
    // XXX translate hex to mVector
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::GetDescription(char* *htmlResult)
{
    // XXX generate some nice html
    return NS_OK;
}

NS_IMETHODIMP
nsBrowsingProfile::CountPageVisit(const char* url)
{
    // Here's where the real work is:
    // Find the url in the directory, and get the category ID.
    // Then increment the count (and set the flags) for that category ID
    // in the vector.
    
    nsresult rv = NS_OK;
    PRBool found = PR_FALSE;
    do {
        nsIRDFResource* urlRes;
        rv = gRDFService->GetResource(url, &urlRes);
        if (NS_FAILED(rv)) return rv;

        nsIRDFAssertionCursor* cursor;
        rv = gCategoryDB->GetSources(gChildProperty, urlRes, PR_TRUE, &cursor);
        if (NS_SUCCEEDED(rv)) {
            while (cursor->Advance()) {
                nsIRDFResource* category;
                rv = cursor->GetSubject(&category);
                if (NS_SUCCEEDED(rv)) {
                    // found this page in a category -- count it
                    nsIRDFNode* catID;
                    rv = gCategoryDB->GetTarget(category, gCategoryIDProperty, PR_TRUE, &catID);
                    if (NS_SUCCEEDED(rv)) {
                        const char* catURI;
                        rv = category->GetValue(&catURI);
                        if (NS_SUCCEEDED(rv)) {
                            nsIRDFInt* catIDInt;
                            rv = catID->QueryInterface(nsIRDFInt::GetIID(), (void**)&catIDInt);
                            if (NS_SUCCEEDED(rv)) {
                                int32 id;
                                rv = catIDInt->GetValue(&id);
                                if (NS_SUCCEEDED(rv)) {
                                    rv = RecordHit(catURI, id);
                                    found = PR_TRUE;
                                }
                                NS_RELEASE(catIDInt);
                            }
                        }
                    }
                    NS_RELEASE(category);
                }
            }
            NS_RELEASE(cursor);
        }
        NS_RELEASE(urlRes);

        // we didn't find this page exactly, but see if some ancestor is there
        if (!found) {
            
        }
    } while (found);

    return rv;
}

nsresult
nsBrowsingProfile::RecordHit(const char* categoryURL, PRInt32 id)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRDFObserver methods:

NS_IMETHODIMP
nsBrowsingProfile::OnAssert(nsIRDFResource* subject,
                            nsIRDFResource* predicate,
                            nsIRDFNode* object)
{
    nsresult rv = NS_OK;
    if (peq(predicate, gPageProperty)) {
        nsIRDFResource* objRes;
        rv = object->QueryInterface(nsIRDFResource::GetIID(), (void**)&objRes);
        if (NS_FAILED(rv)) return rv;
        const char* url;
        rv = objRes->GetValue(&url);
        if (NS_SUCCEEDED(rv)) {
            rv = CountPageVisit(url);
        }
        NS_RELEASE(objRes);
    }
    return rv;
}

NS_IMETHODIMP
nsBrowsingProfile::OnUnassert(nsIRDFResource* subject,
                              nsIRDFResource* predicate,
                              nsIRDFNode* object)
{
    // we don't care about history entries going away
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewBrowsingProfile(const char* userProfileName,
                      nsIBrowsingProfile* *result)
{
    nsresult rv;
    nsBrowsingProfile* profile = new nsBrowsingProfile();
    if (profile == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = profile->Init(userProfileName);
    if (NS_FAILED(rv)) {
        delete profile;
    }
    NS_ADDREF(profile);
    *result = profile;
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
