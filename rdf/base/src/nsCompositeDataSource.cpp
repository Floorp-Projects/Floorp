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

  A simple composite data source implementation. A composit data
  source is just a strategy for combining individual data sources into
  a collective graph.


  1) A composite data source holds a sequence of data sources. The set
     of data sources can be specified during creation of the
     database. Data sources can also be added/deleted from a database
     later.

  2) The aggregation mechanism is based on simple super-positioning of
     the graphs from the datasources. If there is a conflict (i.e., 
     data source A has a true arc from foo to bar while data source B
     has a false arc from foo to bar), the data source that it earlier
     in the sequence wins.

     The implementation below doesn't really do this and needs to be
     fixed.

*/

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "rdf.h"

#ifdef NS_DEBUG
#include "prlog.h"
#include "prprf.h"
#include <stdio.h>
PRLogModuleInfo* nsRDFLog = nsnull;
#endif

static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////
// CompositeDataSourceImpl

class CompositeDataSourceImpl : public nsIRDFCompositeDataSource,
                                public nsIRDFObserver
{
public:
    CompositeDataSourceImpl(void);
    CompositeDataSourceImpl(char** dataSources);

    nsVoidArray mDataSources;

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(char* *uri);

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source);

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsISimpleEnumerator** sources);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsISimpleEnumerator** targets);

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion);

    NS_IMETHOD AddObserver(nsIRDFObserver* n);

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsISimpleEnumerator** labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsISimpleEnumerator** labels);

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aCursor);

    NS_IMETHOD Flush();

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

    // nsIRDFCompositeDataSource interface
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source);
    NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object);

    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

    // Implementation methods
    PRBool HasAssertionN(int n, nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv);
protected:
    nsVoidArray*  mObservers;
        
    virtual ~CompositeDataSourceImpl(void);
};



////////////////////////////////////////////////////////////////////////
//
// CompositeEnumeratorImpl
//

class CompositeEnumeratorImpl : public nsISimpleEnumerator
{
public:
    CompositeEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource);
    virtual ~CompositeEnumeratorImpl();
    
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    // pure abstract methods to be overridden
    virtual nsresult
    GetEnumerator(nsIRDFDataSource* aDataSource, nsISimpleEnumerator** aResult) = 0;

    virtual nsresult
    HasNegation(nsIRDFDataSource* aDataSource, nsIRDFNode* aNode, PRBool* aResult) = 0;

protected:
    CompositeDataSourceImpl* mCompositeDataSource;
    nsISimpleEnumerator* mCurrent;
    nsIRDFNode*       mResult;
    PRInt32           mNext;

    nsVoidArray mAlreadyReturned;
};

        
CompositeEnumeratorImpl::CompositeEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource)
    : mCompositeDataSource(aCompositeDataSource), 
      mCurrent(nsnull),
      mResult(nsnull),
	  mNext(0)
{
	NS_INIT_REFCNT();
    NS_ADDREF(mCompositeDataSource);
}


CompositeEnumeratorImpl::~CompositeEnumeratorImpl(void)
{
    for (PRInt32 i = mAlreadyReturned.Count() - 1; i >= 0; --i) {
        nsIRDFNode* node = (nsIRDFNode*) mAlreadyReturned[i];
        NS_RELEASE(node);
    }

    NS_IF_RELEASE(mCurrent);
    NS_IF_RELEASE(mResult);
    NS_RELEASE(mCompositeDataSource);
}


NS_IMPL_ADDREF(CompositeEnumeratorImpl);
NS_IMPL_RELEASE(CompositeEnumeratorImpl);

NS_IMETHODIMP_(nsresult)
CompositeEnumeratorImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsISimpleEnumerator::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsISimpleEnumerator*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
CompositeEnumeratorImpl::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // If we've already queued up a next target, then yep, there are
    // more elements.
    if (mResult) {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    // Otherwise, we'll need to find a next target, switching cursors
    // if necessary.
    while (mNext < mCompositeDataSource->mDataSources.Count()) {
        if (! mCurrent) {
            // We don't have a current enumerator, so create a new one on
            // the next data source.
            nsIRDFDataSource* datasource =
                (nsIRDFDataSource*) mCompositeDataSource->mDataSources[mNext];

            rv = GetEnumerator(datasource, &mCurrent);
            if (NS_FAILED(rv)) return rv;
        }

        do {
            PRInt32 i;

            PRBool hasMore;
            rv = mCurrent->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            // Is the current enumerator depleted?
            if (! hasMore) {
                NS_RELEASE(mCurrent);
                break;
            }

            // Even if the current enumerator has more elements, we still
            // need to check that the current element isn't masked by
            // a negation in an earlier data source.

            // "Peek" ahead and pull out the next target.
            nsCOMPtr<nsISupports> result;
            rv = mCurrent->GetNext(getter_AddRefs(result));
            if (NS_FAILED(rv)) return rv;

            rv = result->QueryInterface(nsIRDFNode::GetIID(), (void**) &mResult);
            if (NS_FAILED(rv)) return rv;

            // See if any previous data source negates this
            PRBool hasNegation = PR_FALSE;
            for (i = mNext - 1; i >= 0; --i) {
                nsIRDFDataSource* datasource =
                    (nsIRDFDataSource*) mCompositeDataSource->mDataSources[i];

                rv = HasNegation(datasource, mResult, &hasNegation);
                if (NS_FAILED(rv)) return rv;

                if (hasNegation)
                    break;
            }

            // if so, we've gotta keep looking
            if (hasNegation) {
                NS_RELEASE(mResult);
                continue;
            }


            // Now see if we've returned it once already.
            // XXX N.B. performance here...may want to hash if things get large?
            PRBool alreadyReturned = PR_FALSE;
            for (i = mAlreadyReturned.Count() - 1; i >= 0; --i) {
                if (mAlreadyReturned[i] == mResult) {
                    alreadyReturned = PR_TRUE;
                    break;
                }
            }

            if (alreadyReturned) {
                NS_RELEASE(mResult);
                continue;
            }

            // If we get here, then we've really found one. It'll
            // remain cached in mResult until GetNext() sucks it out.
            *aResult = PR_TRUE;
            return NS_OK;
        } while (1);

        ++mNext;
    }

    // if we get here, there aren't any elements left.
    *aResult = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
CompositeEnumeratorImpl::GetNext(nsISupports** aResult)
{
    nsresult rv;

    PRBool hasMore;
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore)
        return NS_ERROR_UNEXPECTED;

    // Don't AddRef: we "transfer" ownership to the caller
    *aResult = mResult;
    mResult = nsnull;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//
// CompositeArcsInOutEnumeratorImpl
//
//

class CompositeArcsInOutEnumeratorImpl : public CompositeEnumeratorImpl
{
public:
    enum Type { eArcsIn, eArcsOut };

    CompositeArcsInOutEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource,
                                     nsIRDFNode* aNode,
                                     Type aType);

    virtual ~CompositeArcsInOutEnumeratorImpl();

    virtual nsresult
    GetEnumerator(nsIRDFDataSource* aDataSource, nsISimpleEnumerator** aResult);

    virtual nsresult
    HasNegation(nsIRDFDataSource* aDataSource, nsIRDFNode* aNode, PRBool* aResult);

private:
    nsIRDFNode* mNode;
    Type        mType;
};

CompositeArcsInOutEnumeratorImpl::CompositeArcsInOutEnumeratorImpl(
                CompositeDataSourceImpl* aCompositeDataSource,
                nsIRDFNode* aNode,
                Type aType)
    : CompositeEnumeratorImpl(aCompositeDataSource),
      mNode(aNode),
      mType(aType)
{
    NS_ADDREF(mNode);
}

CompositeArcsInOutEnumeratorImpl::~CompositeArcsInOutEnumeratorImpl()
{
    NS_RELEASE(mNode);
}


nsresult
CompositeArcsInOutEnumeratorImpl::GetEnumerator(
                 nsIRDFDataSource* aDataSource,
                 nsISimpleEnumerator** aResult)
{
    if (mType == eArcsIn) {
        return aDataSource->ArcLabelsIn(mNode, aResult);
    }
    else {
        nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(mNode) );
        return aDataSource->ArcLabelsOut(resource, aResult);
    }
}

nsresult
CompositeArcsInOutEnumeratorImpl::HasNegation(
                 nsIRDFDataSource* aDataSource,
                 nsIRDFNode* aNode,
                 PRBool* aResult)
{
    *aResult = PR_FALSE;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
//
// CompositeAssertionEnumeratorImpl
//

class CompositeAssertionEnumeratorImpl : public CompositeEnumeratorImpl
{
public:
    CompositeAssertionEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource,
                                     nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget,
                                     PRBool aTruthValue);

    virtual ~CompositeAssertionEnumeratorImpl();

    virtual nsresult
    GetEnumerator(nsIRDFDataSource* aDataSource, nsISimpleEnumerator** aResult);

    virtual nsresult
    HasNegation(nsIRDFDataSource* aDataSource, nsIRDFNode* aNode, PRBool* aResult);

private:
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    PRBool          mTruthValue;
};


CompositeAssertionEnumeratorImpl::CompositeAssertionEnumeratorImpl(
                  CompositeDataSourceImpl* aCompositeDataSource,
                  nsIRDFResource* aSource,
                  nsIRDFResource* aProperty,
                  nsIRDFNode* aTarget,
                  PRBool aTruthValue)
    : CompositeEnumeratorImpl(aCompositeDataSource),
      mSource(aSource),
      mProperty(aProperty),
      mTarget(aTarget),
      mTruthValue(aTruthValue)
{
    NS_IF_ADDREF(mSource);
    NS_ADDREF(mProperty); // always must be specified
    NS_IF_ADDREF(mTarget);
}

CompositeAssertionEnumeratorImpl::~CompositeAssertionEnumeratorImpl()
{
    NS_IF_RELEASE(mSource);
    NS_RELEASE(mProperty);
    NS_IF_RELEASE(mTarget);
}


nsresult
CompositeAssertionEnumeratorImpl::GetEnumerator(
                 nsIRDFDataSource* aDataSource,
                 nsISimpleEnumerator** aResult)
{
    if (mSource) {
        return aDataSource->GetTargets(mSource, mProperty, mTruthValue, aResult);
    }
    else {
        return aDataSource->GetSources(mProperty, mTarget, mTruthValue, aResult);
    }
}

nsresult
CompositeAssertionEnumeratorImpl::HasNegation(
                 nsIRDFDataSource* aDataSource,
                 nsIRDFNode* aNode,
                 PRBool* aResult)
{
    if (mSource) {
        return aDataSource->HasAssertion(mSource, mProperty, aNode, !mTruthValue, aResult);
    }
    else {
        nsCOMPtr<nsIRDFResource> source( do_QueryInterface(aNode) );
        return aDataSource->HasAssertion(source, mProperty, mTarget, !mTruthValue, aResult);
    }
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFCompositeDataSource(nsIRDFCompositeDataSource** result)
{
    CompositeDataSourceImpl* db = new CompositeDataSourceImpl();
    if (! db)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = db;
    NS_ADDREF(*result);
    return NS_OK;
}


CompositeDataSourceImpl::CompositeDataSourceImpl(void)
    : mObservers(nsnull)
{
    NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (nsRDFLog == nsnull) 
        nsRDFLog = PR_NewLogModule("RDF");
#endif
}


CompositeDataSourceImpl::~CompositeDataSourceImpl(void)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->RemoveObserver(this);
        NS_IF_RELEASE(ds);
    }
    delete mObservers;
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(CompositeDataSourceImpl);
NS_IMPL_RELEASE(CompositeDataSourceImpl);

NS_IMETHODIMP
CompositeDataSourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIRDFCompositeDataSource::GetIID()) ||
        iid.Equals(nsIRDFDataSource::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
		NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(nsIRDFObserver::GetIID())) {
        *result = NS_STATIC_CAST(nsIRDFObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource interface

NS_IMETHODIMP
CompositeDataSourceImpl::Init(const char* uri)
{
    NS_NOTREACHED("CompositeDataSourceImpl::Init");
    return NS_ERROR_UNEXPECTED; // XXX CompositeDataSourceImpl doesn't have a URI?
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetURI(char* *uri)
{
    NS_NOTREACHED("CompositeDataSourceImpl::GetURI");
    return NS_ERROR_UNEXPECTED; // XXX CompositeDataSourceImpl doesn't have a URI?
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetSource(nsIRDFResource* property,
                                   nsIRDFNode* target,
                                   PRBool tv,
                                   nsIRDFResource** source)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        nsresult rv;
        rv = ds->GetSource(property, target, tv, source);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_RDF_NO_VALUE)
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in a more local data source
        if (!HasAssertionN(count-1, *source, property, target, !tv)) 
            return NS_OK;

        NS_RELEASE(*source);
        return NS_RDF_NO_VALUE;
    }
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetSources(nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue,
                                    nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = new CompositeAssertionEnumeratorImpl(this, nsnull, aProperty, aTarget, aTruthValue);
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetTarget(nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   PRBool aTruthValue,
                                   nsIRDFNode** aResult)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        nsresult rv;
        rv = ds->GetTarget(aSource, aProperty, aTruthValue, aResult);
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_OK) {
            // okay, found it. make sure we don't have the opposite
            // asserted in an earlier data source

            if (HasAssertionN(count-1, aSource, aProperty, *aResult, !aTruthValue)) {
                // whoops, it's been negated.
                NS_RELEASE(*aResult);
                return NS_RDF_NO_VALUE;
            }
            else {
                return NS_OK;
            }
        }
    }

    // Otherwise, we couldn't find it at all.
    return NS_RDF_NO_VALUE;
}

PRBool
CompositeDataSourceImpl::HasAssertionN(int n,
                                       nsIRDFResource* aSource,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aTarget,
                                       PRBool aTruthValue)
{
    nsresult rv;
    for (PRInt32 m = 0; m < n; ++m) {
        nsIRDFDataSource* datasource = (nsIRDFDataSource*) mDataSources[m];

        PRBool result;
        rv = datasource->HasAssertion(aSource, aProperty, aTarget, aTruthValue, &result);
        if (NS_FAILED(rv))
            return PR_FALSE;

        // found it!
        if (result)
            return PR_TRUE;
    }
    return PR_FALSE;
}
    


NS_IMETHODIMP
CompositeDataSourceImpl::GetTargets(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    PRBool aTruthValue,
                                    nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = new CompositeAssertionEnumeratorImpl(this, aSource, aProperty, nsnull, aTruthValue);
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Assert(nsIRDFResource* aSource, 
                                nsIRDFResource* aProperty, 
                                nsIRDFNode* aTarget,
                                PRBool aTruthValue)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // XXX Need to add back the stuff for unblocking ...

    // We iterate backwards from the last data source which was added
    // ("the most remote") to the first ("the most local"), trying to
    // apply the assertion in each.
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->Assert(aSource, aProperty, aTarget, aTruthValue);
        if (NS_RDF_ASSERTION_ACCEPTED == rv)
            return rv;

        if (NS_FAILED(rv))
            return rv;
    }

    // nobody wanted to accept it
    return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Unassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Iterate through each of the datasources, starting with "the
    // most local" and moving to "the most remote". If _any_ of the
    // datasources have the assertion, attempt to unassert it.
    PRBool unasserted = PR_TRUE;
    PRInt32 i;
    PRInt32 count = mDataSources.Count();
    for (i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        PRBool hasAssertion;
        rv = ds->HasAssertion(aSource, aProperty, aTarget, PR_TRUE, &hasAssertion);
        if (NS_FAILED(rv)) return rv;

        if (hasAssertion) {
            rv = ds->Unassert(aSource, aProperty, aTarget);
            if (NS_FAILED(rv)) return rv;

            if (rv != NS_RDF_ASSERTION_ACCEPTED) {
                unasserted = PR_FALSE;
                break;
            }
        }
    }

    // Either none of the datasources had it, or they were all willing
    // to let it be unasserted.
    if (unasserted)
        return NS_RDF_ASSERTION_ACCEPTED;

    // If we get here, one of the datasources already had the
    // assertion, and was adamant about not letting us remove
    // it. Iterate from the "most local" to the "most remote"
    // attempting to assert the negation...
    for (i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->Assert(aSource, aProperty, aTarget, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        // Did it take?
        if (rv == NS_RDF_ASSERTION_ACCEPTED)
            return rv;
    }

    // Couln't get anyone to accept the negation, either.
    return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
CompositeDataSourceImpl::HasAssertion(nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget,
                                      PRBool aTruthValue,
                                      PRBool* aResult)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // Otherwise, look through all the data sources to see if anyone
    // has the positive...
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* datasource = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = datasource->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
        if (NS_FAILED(rv)) return rv;

        if (*aResult)
            return NS_OK;

        PRBool hasNegation;
        rv = datasource->HasAssertion(aSource, aProperty, aTarget, !aTruthValue, &hasNegation);
        if (NS_FAILED(rv)) return rv;

        if (hasNegation) {
            *aResult = PR_FALSE;
            return NS_OK;
        }
    }

    // If we get here, nobody had the assertion at all
    *aResult = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::AddObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (!mObservers) {
        if ((mObservers = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX ensure uniqueness?

    mObservers->AppendElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::RemoveObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (!mObservers)
        return NS_OK;

    mObservers->RemoveElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::ArcLabelsIn(nsIRDFNode* aTarget, nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsISimpleEnumerator* result = 
        new CompositeArcsInOutEnumeratorImpl(this, aTarget, CompositeArcsInOutEnumeratorImpl::eArcsIn);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::ArcLabelsOut(nsIRDFResource* aSource,
                                      nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsISimpleEnumerator* result =
        new CompositeArcsInOutEnumeratorImpl(this, aSource, CompositeArcsInOutEnumeratorImpl::eArcsOut);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetAllResources(nsISimpleEnumerator** aResult)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Flush()
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->Flush();
    }
    return NS_OK;
}

#if 0
NS_IMETHODIMP
CompositeDataSourceImpl::GetEnabledCommands(nsISupportsArray* aSources,
                                            nsISupportsArray* aArguments,
                                            nsIEnumerator**   aResult)
{
    nsCOMPtr<nsIEnumerator> commands;        // union of enabled commands
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsIEnumerator* dsCmds;
        nsresult rv = ds->GetEnabledCommands(aSources, aArguments, &dsCmds);
        if (NS_SUCCEEDED(rv)) {
            if (commands == nsnull) {
                commands = dont_QueryInterface(dsCmds);
            }
            else {
                nsIEnumerator* unionCmds;
                rv = NS_NewUnionEnumerator(commands, dsCmds, &unionCmds);
                if (NS_FAILED(rv)) return rv;
                NS_RELEASE(dsCmds);
                commands = dont_QueryInterface(unionCmds);
            }
        }
    }
    *aResult = commands;
    return NS_OK;
}
#endif

NS_IMETHODIMP
CompositeDataSourceImpl::GetAllCommands(nsIRDFResource* source,
                                        nsIEnumerator/*<nsIRDFResource>*/** result)
{
    nsCOMPtr<nsIEnumerator> commands;        // union of enabled commands
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsIEnumerator* dsCmds;
        nsresult rv = ds->GetAllCommands(source, &dsCmds);
        if (NS_SUCCEEDED(rv)) {
            if (commands == nsnull) {
                commands = dont_QueryInterface(dsCmds);
            }
            else {
                nsIEnumerator* unionCmds;
                rv = NS_NewUnionEnumerator(commands, dsCmds, &unionCmds);
                if (NS_FAILED(rv)) return rv;
                NS_RELEASE(dsCmds);
                commands = dont_QueryInterface(unionCmds);
            }
        }
    }
    *result = commands;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                          nsIRDFResource*   aCommand,
                                          nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                          PRBool* aResult)
{
    nsresult rv;
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        PRBool enabled;
        rv = ds->IsCommandEnabled(aSources, aCommand, aArguments, &enabled);
        if (NS_FAILED(rv)) return rv;

        if (! enabled) {
            *aResult = PR_FALSE;
            return NS_OK;
        }
    }
    *aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                   nsIRDFResource*   aCommand,
                                   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsresult rv = ds->DoCommand(aSources, aCommand, aArguments);
        if (NS_FAILED(rv)) return rv;   // all datasources must succeed
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFCompositeDataSource methods
// XXX rvg We should make this take an additional argument specifying where
// in the sequence of data sources (of the db), the new data source should
// fit in. Right now, the new datasource gets stuck at the end.
// need to add the observers of the CompositeDataSourceImpl to the new data source.

NS_IMETHODIMP
CompositeDataSourceImpl::AddDataSource(nsIRDFDataSource* source)
{
    NS_ASSERTION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    mDataSources.InsertElementAt(source, 0);
    source->AddObserver(this);
    NS_ADDREF(source);
    return NS_OK;
}



NS_IMETHODIMP
CompositeDataSourceImpl::RemoveDataSource(nsIRDFDataSource* source)
{
    NS_ASSERTION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;


    if (mDataSources.IndexOf(source) >= 0) {
        mDataSources.RemoveElement(source);
        source->RemoveObserver(this);
        NS_RELEASE(source);
    }
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::OnAssert(nsIRDFResource* subject,
                                  nsIRDFResource* predicate,
                                  nsIRDFNode* object)
{
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnAssert(subject, predicate, object);
            // XXX ignore return value?
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::OnUnassert(nsIRDFResource* subject,
                                    nsIRDFResource* predicate,
                                    nsIRDFNode* object)
{
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(subject, predicate, object);
            // XXX ignore return value?
        }
    }
    return NS_OK;
}



