/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  Implementation for the local store

 */

#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIComponentManager.h"
#include "nsILocalStore.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFXMLDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "rdf.h"

////////////////////////////////////////////////////////////////////////

class LocalStoreImpl : public nsILocalStore,
                       public nsIRDFDataSource
{
private:
    nsIRDFXMLDataSource* mInner;
    char* mURI;

public:
    LocalStoreImpl();
    virtual ~LocalStoreImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsILocalStore interface

    // nsIRDFDataSource interface. Most of these are just delegated to
    // the inner, in-memory datasource.
    NS_IMETHOD Init(const char* aURI);

    NS_IMETHOD GetURI(char* *aURI);

    NS_IMETHOD GetSource(nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget,
                         PRBool aTruthValue,
                         nsIRDFResource** aSource) {
        return mInner->GetSource(aProperty, aTarget, aTruthValue, aSource);
    }

    NS_IMETHOD GetSources(nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget,
                          PRBool aTruthValue,
                          nsIRDFAssertionCursor** aSources) {
        return mInner->GetSources(aProperty, aTarget, aTruthValue, aSources);
    }

    NS_IMETHOD GetTarget(nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         PRBool aTruthValue,
                         nsIRDFNode** aTarget) {
        return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
    }

    NS_IMETHOD GetTargets(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          PRBool aTruthValue,
                          nsIRDFAssertionCursor** aTargets) {
        return mInner->GetTargets(aSource, aProperty, aTruthValue, aTargets);
    }

    NS_IMETHOD Assert(nsIRDFResource* aSource, 
                      nsIRDFResource* aProperty, 
                      nsIRDFNode* aTarget,
                      PRBool aTruthValue) {
        return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
    }

    NS_IMETHOD Unassert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget) {
        return mInner->Unassert(aSource, aProperty, aTarget);
    }

    NS_IMETHOD HasAssertion(nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode* aTarget,
                            PRBool aTruthValue,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* aObserver) {
        return mInner->AddObserver(aObserver);
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver) {
        return mInner->RemoveObserver(aObserver);
    }

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* aNode,
                           nsIRDFArcsInCursor** aLabels) {
        return mInner->ArcLabelsIn(aNode, aLabels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* aSource,
                            nsIRDFArcsOutCursor** aLabels) {
        return mInner->ArcLabelsOut(aSource, aLabels);
    }

    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor) {
        return mInner->GetAllResources(aCursor);
    }

    NS_IMETHOD Flush(void) {
        return mInner->Flush();
    }

    NS_IMETHOD GetAllCommands(nsIRDFResource* aSource,
                              nsIEnumerator/*<nsIRDFResource>*/** aCommands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

};


////////////////////////////////////////////////////////////////////////


LocalStoreImpl::LocalStoreImpl(void)
    : mInner(nsnull), mURI(nsnull)
{
    NS_INIT_ISUPPORTS();
}

LocalStoreImpl::~LocalStoreImpl(void)
{
    Flush();
    NS_IF_RELEASE(mInner);
    PL_strfree(mURI);
}


PR_IMPLEMENT(nsresult)
NS_NewLocalStore(nsILocalStore** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    LocalStoreImpl* impl = new LocalStoreImpl();
    if (! impl)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(impl);
    *aResult = impl;
    return NS_OK;
}


// nsISupports interface

NS_IMPL_ADDREF(LocalStoreImpl);
NS_IMPL_RELEASE(LocalStoreImpl);

NS_IMETHODIMP
LocalStoreImpl::QueryInterface(REFNSIID aIID, void** aResult)
{
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(kISupportsIID) ||
        aIID.Equals(nsILocalStore::GetIID())) {
        *aResult = NS_STATIC_CAST(nsILocalStore*, this);
    }
    else if (aIID.Equals(nsIRDFDataSource::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIRDFDataSource *, this);
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }

    NS_ADDREF(this);
    return NS_OK;
}


// nsILocalStore interface



// nsIRDFDataSource interface

NS_IMETHODIMP
LocalStoreImpl::Init(const char* aURI)
{
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,       NS_RDFSERVICE_CID);

    nsresult rv;

    // XXX use profile dir or something
    nsSpecialSystemDirectory spec(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
    spec += "localstore.rdf";

    if (! spec.Exists()) {
        nsOutputFileStream os(spec);
        os << "<?xml version=\"1.0\"?>" << nsEndl;
        os << "<RDF:RDF xmlns:RDF=\"" << RDF_NAMESPACE_URI << "\"" << nsEndl;
        os << "         xmlns:NC=\""  << NC_NAMESPACE_URI  << "\">" << nsEndl;
        os << "  <!-- Empty -->" << nsEndl;
        os << "</RDF:RDF>" << nsEndl;
    }

    mURI = PL_strdup(aURI);
    if (! mURI)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                            nsnull,
                                            nsIRDFXMLDataSource::GetIID(),
                                            (void**) &mInner);
    if (NS_FAILED(rv)) return rv;

    rv = mInner->Init((const char*) nsFileURL(spec));
    if (NS_FAILED(rv)) return rv;

    // register this as a named data source with the RDF service
    nsIRDFService* rdf;
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      nsIRDFService::GetIID(),
                                      (nsISupports**) &rdf);

    if (NS_FAILED(rv)) return rv;

    rv = rdf->RegisterDataSource(this, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register local store");

    nsServiceManager::ReleaseService(kRDFServiceCID, rdf);
    return rv;
}



NS_IMETHODIMP
LocalStoreImpl::GetURI(char* *aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    *aURI = nsXPIDLCString::Copy(mURI);
    if (! *aURI)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}



NS_IMETHODIMP
LocalStoreImpl::GetAllCommands(nsIRDFResource* aSource,
                               nsIEnumerator/*<nsIRDFResource>*/** aCommands)
{
    // XXX Although this is the wrong thing to do, it works. I'll file a
    // bug to fix it.
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
LocalStoreImpl::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                 nsIRDFResource*   aCommand,
                                 nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                 PRBool* aResult)
{
    *aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
LocalStoreImpl::DoCommand(nsISupportsArray* aSources,
                          nsIRDFResource*   aCommand,
                          nsISupportsArray* aArguments)
{
    // no-op
    return NS_OK;
}
