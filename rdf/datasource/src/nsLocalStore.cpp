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

/*

  Implementation for the local store

 */

#include "nsIFileSpec.h"
#include "nsFileStream.h"
#include "nsIComponentManager.h"
#include "nsILocalStore.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "rdf.h"
#include "nsCOMPtr.h"
#include "nsAppDirectoryServiceDefs.h"


////////////////////////////////////////////////////////////////////////

class LocalStoreImpl : public nsILocalStore,
                       public nsIRDFDataSource,
                       public nsIRDFRemoteDataSource
{
private:
    nsCOMPtr<nsIRDFDataSource> mInner;

    LocalStoreImpl();
    virtual ~LocalStoreImpl();
    nsresult Init();

    friend nsresult
    NS_NewLocalStore(nsILocalStore** aResult);

    nsCOMPtr<nsISupportsArray> mObservers;

public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsILocalStore interface

    // nsIRDFDataSource interface. Most of these are just delegated to
    // the inner, in-memory datasource.
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
                          nsISimpleEnumerator** aSources) {
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
                          nsISimpleEnumerator** aTargets) {
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

    NS_IMETHOD Change(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aOldTarget,
                      nsIRDFNode* aNewTarget) {
        return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
    }

    NS_IMETHOD Move(nsIRDFResource* aOldSource,
                    nsIRDFResource* aNewSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aTarget) {
        return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
    }

    NS_IMETHOD HasAssertion(nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode* aTarget,
                            PRBool aTruthValue,
                            PRBool* hasAssertion) {
        return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, hasAssertion);
    }

    NS_IMETHOD AddObserver(nsIRDFObserver* aObserver) {
        // Observers are _never_ notified, but we still have to play
        // nice.
        if (! mObservers) {
            nsresult rv;
            rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
            if (NS_FAILED(rv)) return rv;
        }

        mObservers->AppendElement(aObserver);
        return NS_OK;
    }

    NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver) {
        if (mObservers) {
            mObservers->RemoveElement(aObserver);
        }
        return NS_OK;
    }

    NS_IMETHOD HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *_retval) {
        return mInner->HasArcIn(aNode, aArc, _retval);
    }

    NS_IMETHOD HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *_retval) {
        return mInner->HasArcOut(aSource, aArc, _retval);
    }

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* aNode,
                           nsISimpleEnumerator** aLabels) {
        return mInner->ArcLabelsIn(aNode, aLabels);
    }

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* aSource,
                            nsISimpleEnumerator** aLabels) {
        return mInner->ArcLabelsOut(aSource, aLabels);
    }

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult) {
        return mInner->GetAllResources(aResult);
    }

    NS_IMETHOD GetAllCommands(nsIRDFResource* aSource,
                              nsIEnumerator/*<nsIRDFResource>*/** aCommands);

    NS_IMETHOD GetAllCmds(nsIRDFResource* aSource,
                              nsISimpleEnumerator/*<nsIRDFResource>*/** aCommands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

    NS_IMETHOD GetLoaded(PRBool* _result);
	NS_IMETHOD Init(const char *uri);
	NS_IMETHOD Flush();
	NS_IMETHOD Refresh(PRBool sync);
};


////////////////////////////////////////////////////////////////////////


LocalStoreImpl::LocalStoreImpl(void)
{
    NS_INIT_ISUPPORTS();
}

LocalStoreImpl::~LocalStoreImpl(void)
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: LocalStoreImpl\n", gInstanceCount);
#endif
}


nsresult
NS_NewLocalStore(nsILocalStore** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    LocalStoreImpl* impl = new LocalStoreImpl();
    if (! impl)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = impl->Init();
    if (NS_FAILED(rv)) {
        delete impl;
        return rv;
    }

    // We need to read this synchronously.
    rv = impl->Refresh(PR_TRUE);

    if (NS_FAILED(rv)) {

#ifdef	DEBUG
	printf("\n\nRDF: NS_NewLocalStore::Refresh() failed.\n\n");
#endif

        delete impl;
        return rv;
    }

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
        aIID.Equals(NS_GET_IID(nsILocalStore))) {
        *aResult = NS_STATIC_CAST(nsILocalStore*, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsIRDFDataSource))) {
        *aResult = NS_STATIC_CAST(nsIRDFDataSource *, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsIRDFRemoteDataSource))) {
        *aResult = NS_STATIC_CAST(nsIRDFRemoteDataSource *, this);
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
LocalStoreImpl::GetLoaded(PRBool* _result)
{
	nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mInner);
    NS_ASSERTION(remote != nsnull, "not an nsIRDFRemoteDataSource");
	if (! remote)
        return NS_ERROR_UNEXPECTED;

    return remote->GetLoaded(_result);
}


NS_IMETHODIMP
LocalStoreImpl::Init(const char *uri)
{
	return(NS_OK);
}

NS_IMETHODIMP
LocalStoreImpl::Flush()
{
	nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mInner);
    NS_ASSERTION(remote != nsnull, "not an nsIRDFRemoteDataSource");
	if (! remote)
        return NS_ERROR_UNEXPECTED;

    return remote->Flush();
}

NS_IMETHODIMP
LocalStoreImpl::Refresh(PRBool sync)
{
	nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mInner);
    NS_ASSERTION(remote != nsnull, "not an nsIRDFRemoteDataSource");
	if (! remote)
        return NS_ERROR_UNEXPECTED;

    return remote->Refresh(sync);
}

nsresult
LocalStoreImpl::Init()
{
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,       NS_RDFSERVICE_CID);

    nsresult rv;

    // Look for localstore.rdf in the current profile
    // directory. Bomb if we can't find it.

    nsCOMPtr<nsIFile> aFile;
    nsCOMPtr<nsIFileSpec> tempSpec;

    rv = NS_GetSpecialDirectory(NS_APP_LOCALSTORE_50_FILE, getter_AddRefs(aFile));
    if (NS_FAILED(rv)) return rv;

    // Turn the nsIFile into a nsFileSpec.
    // This is evil! nsOutputFileStream needs
    // to take an nsILocalFile (bug #46394)
    nsXPIDLCString pathBuf;
    rv = aFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;
    nsFileSpec spec((const char *)pathBuf);

    if (! spec.Exists()) {
        {
            nsOutputFileStream os(spec);

            os << "<?xml version=\"1.0\"?>" << nsEndl;
            os << "<RDF:RDF xmlns:RDF=\"" << RDF_NAMESPACE_URI << "\"" << nsEndl;
            os << "         xmlns:NC=\""  << NC_NAMESPACE_URI  << "\">" << nsEndl;
            os << "  <!-- Empty -->" << nsEndl;
            os << "</RDF:RDF>" << nsEndl;
        }

        // Okay, now see if the file exists _for real_. If it's still
        // not there, it could be that the profile service gave us
        // back a read-only directory. Whatever.
        if (! spec.Exists())
            return NS_ERROR_UNEXPECTED;
    }

    rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                            nsnull,
                                            NS_GET_IID(nsIRDFDataSource),
                                            (void**) &mInner);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(mInner);

    rv = remote->Init((const char*) nsFileURL(spec));
    if (NS_FAILED(rv)) return rv;

    // register this as a named data source with the RDF service
    NS_WITH_SERVICE(nsIRDFService, rdf, kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = rdf->RegisterDataSource(this, PR_FALSE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register local store");
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}



NS_IMETHODIMP
LocalStoreImpl::GetURI(char* *aURI)
{
    NS_PRECONDITION(aURI != nsnull, "null ptr");
    if (! aURI)
        return NS_ERROR_NULL_POINTER;

    *aURI = nsXPIDLCString::Copy("rdf:localstore");
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
LocalStoreImpl::GetAllCmds(nsIRDFResource* aSource,
                               nsISimpleEnumerator/*<nsIRDFResource>*/** aCommands)
{
	return(NS_NewEmptyEnumerator(aCommands));
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
