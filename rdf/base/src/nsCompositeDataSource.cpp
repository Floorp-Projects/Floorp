/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ramanathan Guha <guha@netscape.com>
 *   Chris Waterson <waterson@netscape.com
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include <new.h>
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsFixedSizeAllocator.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "rdf.h"

#include "nsEnumeratorUtils.h"

#ifdef NS_DEBUG
#include "prlog.h"
#include "prprf.h"
#include <stdio.h>
PRLogModuleInfo* nsRDFLog = nsnull;
#endif

static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

//----------------------------------------------------------------------
//
// CompositeDataSourceImpl
//

class CompositeEnumeratorImpl;
class CompositeArcsInOutEnumeratorImpl;
class CompositeAssertionEnumeratorImpl;

class CompositeDataSourceImpl : public nsIRDFCompositeDataSource,
                                public nsIRDFObserver
{
public:
    CompositeDataSourceImpl(void);
    CompositeDataSourceImpl(char** dataSources);

    nsAutoVoidArray mDataSources;

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_DECL_NSIRDFDATASOURCE

    // nsIRDFCompositeDataSource interface
    NS_DECL_NSIRDFCOMPOSITEDATASOURCE

    // nsIRDFObserver interface
    NS_DECL_NSIRDFOBSERVER

    PRBool HasAssertionN(int n, nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv);

protected:
	nsAutoVoidArray mObservers;

	PRBool      mAllowNegativeAssertions;
	PRBool      mCoalesceDuplicateArcs;
    PRInt32     mUpdateBatchNest;

    nsFixedSizeAllocator mAllocator;

	virtual ~CompositeDataSourceImpl() {}

    friend class CompositeEnumeratorImpl;
    friend class CompositeArcsInOutEnumeratorImpl;
    friend class CompositeAssertionEnumeratorImpl;
};

//----------------------------------------------------------------------
//
// CompositeEnumeratorImpl
//

class CompositeEnumeratorImpl : public nsISimpleEnumerator
{
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    // pure abstract methods to be overridden
    virtual nsresult
    GetEnumerator(nsIRDFDataSource* aDataSource, nsISimpleEnumerator** aResult) = 0;

    virtual nsresult
    HasNegation(nsIRDFDataSource* aDataSource, nsIRDFNode* aNode, PRBool* aResult) = 0;

    virtual void Destroy() = 0;

protected:
    CompositeEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource,
                            PRBool aAllowNegativeAssertions,
                            PRBool aCoalesceDuplicateArcs);

    virtual ~CompositeEnumeratorImpl();
    
    CompositeDataSourceImpl* mCompositeDataSource;

    nsISimpleEnumerator* mCurrent;
    nsIRDFNode*  mResult;
    PRInt32      mNext;
    nsAutoVoidArray  mAlreadyReturned;
    PRPackedBool mAllowNegativeAssertions;
    PRPackedBool mCoalesceDuplicateArcs;
};


CompositeEnumeratorImpl::CompositeEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource,
                                                 PRBool aAllowNegativeAssertions,
                                                 PRBool aCoalesceDuplicateArcs)
    : mCompositeDataSource(aCompositeDataSource),
      mCurrent(nsnull),
      mResult(nsnull),
	  mNext(0),
      mAllowNegativeAssertions(aAllowNegativeAssertions),
      mCoalesceDuplicateArcs(aCoalesceDuplicateArcs)
{
	NS_INIT_REFCNT();
	NS_ADDREF(mCompositeDataSource);
}


CompositeEnumeratorImpl::~CompositeEnumeratorImpl(void)
{
	if (mCoalesceDuplicateArcs == PR_TRUE)
	{
		for (PRInt32 i = mAlreadyReturned.Count() - 1; i >= 0; --i)
		{
			nsIRDFNode *node = (nsIRDFNode *) mAlreadyReturned[i];
			NS_RELEASE(node);
		}
	}

	NS_IF_RELEASE(mCurrent);
	NS_IF_RELEASE(mResult);
	NS_RELEASE(mCompositeDataSource);
}

NS_IMPL_ADDREF(CompositeEnumeratorImpl)
NS_IMPL_RELEASE_WITH_DESTROY(CompositeEnumeratorImpl, Destroy())
NS_IMPL_QUERY_INTERFACE1(CompositeEnumeratorImpl, nsISimpleEnumerator);

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
    for ( ; mNext < mCompositeDataSource->mDataSources.Count(); ++mNext) {
        if (! mCurrent) {
            // We don't have a current enumerator, so create a new one on
            // the next data source.
            nsIRDFDataSource* datasource =
                (nsIRDFDataSource*) mCompositeDataSource->mDataSources[mNext];

            rv = GetEnumerator(datasource, &mCurrent);
            if (NS_FAILED(rv)) return rv;

            NS_ASSERTION(mCurrent != nsnull, "you're always supposed to return an enumerator from GetEnumerator, punk.");
            if (! mCurrent)
                continue;
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

            rv = result->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) &mResult);
            if (NS_FAILED(rv)) return rv;

		if (mAllowNegativeAssertions == PR_TRUE)
		{
			// See if any previous data source negates this
			PRBool hasNegation = PR_FALSE;
			for (i = mNext - 1; i >= 0; --i)
			{
				nsIRDFDataSource* datasource =
				(nsIRDFDataSource*) mCompositeDataSource->mDataSources[i];

				rv = HasNegation(datasource, mResult, &hasNegation);
				if (NS_FAILED(rv)) return rv;

				if (hasNegation)
					break;
			}

			// if so, we've gotta keep looking
			if (hasNegation)
			{
				NS_RELEASE(mResult);
				continue;
			}
		}

	if (mCoalesceDuplicateArcs == PR_TRUE)
	{
		// Now see if we've returned it once already.
		// XXX N.B. performance here...may want to hash if things get large?
		PRBool alreadyReturned = PR_FALSE;
		for (i = mAlreadyReturned.Count() - 1; i >= 0; --i)
		{               
			if (mAlreadyReturned[i] == mResult)
			{
				alreadyReturned = PR_TRUE;
				break;
			}
		}
		if (alreadyReturned == PR_TRUE)
		{
			NS_RELEASE(mResult);
			continue;
		}
	}

            // If we get here, then we've really found one. It'll
            // remain cached in mResult until GetNext() sucks it out.
            *aResult = PR_TRUE;

	// Remember that we returned it, so we don't return duplicates.

	// XXX I wonder if we should make unique-checking be
	// optional. This could get to be pretty expensive (this
	// implementation turns iteration into O(n^2)).

	if (mCoalesceDuplicateArcs == PR_TRUE)
	{
		mAlreadyReturned.AppendElement(mResult);
		NS_ADDREF(mResult);
	}

            return NS_OK;
        } while (1);
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

//----------------------------------------------------------------------
//
// CompositeArcsInOutEnumeratorImpl
//
//

class CompositeArcsInOutEnumeratorImpl : public CompositeEnumeratorImpl
{
public:
    enum Type { eArcsIn, eArcsOut };

    static CompositeArcsInOutEnumeratorImpl*
    Create(nsFixedSizeAllocator& aAllocator,
           CompositeDataSourceImpl* aCompositeDataSource,
           nsIRDFNode* aNode,
           Type aType,
           PRBool aAllowNegativeAssertions,
           PRBool aCoalesceDuplicateArcs) {
        void* place = aAllocator.Alloc(sizeof(CompositeArcsInOutEnumeratorImpl));
        return place
            ? ::new (place) CompositeArcsInOutEnumeratorImpl(aCompositeDataSource,
                                                             aNode, aType,
                                                             aAllowNegativeAssertions,
                                                             aCoalesceDuplicateArcs)
            : nsnull; }

    virtual ~CompositeArcsInOutEnumeratorImpl();

    virtual nsresult
    GetEnumerator(nsIRDFDataSource* aDataSource, nsISimpleEnumerator** aResult);

    virtual nsresult
    HasNegation(nsIRDFDataSource* aDataSource, nsIRDFNode* aNode, PRBool* aResult);

    virtual void Destroy();

protected:
    CompositeArcsInOutEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource,
                                     nsIRDFNode* aNode,
                                     Type aType,
                                     PRBool aAllowNegativeAssertions,
                                     PRBool aCoalesceDuplicateArcs);

private:
    nsIRDFNode* mNode;
    Type        mType;
    PRBool	    mAllowNegativeAssertions;
    PRBool      mCoalesceDuplicateArcs;

    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) { return 0; }
    static void operator delete(void*, size_t) {}
};


CompositeArcsInOutEnumeratorImpl::CompositeArcsInOutEnumeratorImpl(
                CompositeDataSourceImpl* aCompositeDataSource,
                nsIRDFNode* aNode,
                Type aType,
                PRBool aAllowNegativeAssertions,
                PRBool aCoalesceDuplicateArcs)
    : CompositeEnumeratorImpl(aCompositeDataSource, aAllowNegativeAssertions, aCoalesceDuplicateArcs),
      mNode(aNode),
      mType(aType),
      mAllowNegativeAssertions(aAllowNegativeAssertions),
      mCoalesceDuplicateArcs(aCoalesceDuplicateArcs)
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

void
CompositeArcsInOutEnumeratorImpl::Destroy()
{
    // Keep the datasource alive for the duration of the stack
    // frame so its allocator stays valid.
    nsCOMPtr<nsIRDFCompositeDataSource> kungFuDeathGrip =
        dont_QueryInterface(mCompositeDataSource);

    nsFixedSizeAllocator& pool = mCompositeDataSource->mAllocator;
    this->~CompositeArcsInOutEnumeratorImpl();
    pool.Free(this, sizeof(*this));
}


//----------------------------------------------------------------------
//
// CompositeAssertionEnumeratorImpl
//

class CompositeAssertionEnumeratorImpl : public CompositeEnumeratorImpl
{
public:
    static CompositeAssertionEnumeratorImpl*
    Create(nsFixedSizeAllocator& aAllocator,
           CompositeDataSourceImpl* aCompositeDataSource,
           nsIRDFResource* aSource,
           nsIRDFResource* aProperty,
           nsIRDFNode* aTarget,
           PRBool aTruthValue,
           PRBool aAllowNegativeAssertions,
           PRBool aCoalesceDuplicateArcs) {
        void* place = aAllocator.Alloc(sizeof(CompositeAssertionEnumeratorImpl));
        return place
            ? ::new (place) CompositeAssertionEnumeratorImpl(aCompositeDataSource,
                                                             aSource, aProperty, aTarget,
                                                             aTruthValue,
                                                             aAllowNegativeAssertions,
                                                             aCoalesceDuplicateArcs)
            : nsnull; }

    virtual nsresult
    GetEnumerator(nsIRDFDataSource* aDataSource, nsISimpleEnumerator** aResult);

    virtual nsresult
    HasNegation(nsIRDFDataSource* aDataSource, nsIRDFNode* aNode, PRBool* aResult);

    virtual void Destroy();

protected:
    CompositeAssertionEnumeratorImpl(CompositeDataSourceImpl* aCompositeDataSource,
                                     nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget,
                                     PRBool aTruthValue,
                                     PRBool aAllowNegativeAssertions,
                                     PRBool aCoalesceDuplicateArcs);

    virtual ~CompositeAssertionEnumeratorImpl();

private:
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    PRBool          mTruthValue;
    PRBool          mAllowNegativeAssertions;
    PRBool          mCoalesceDuplicateArcs;

    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) { return 0; }
    static void operator delete(void*, size_t) {}
};


CompositeAssertionEnumeratorImpl::CompositeAssertionEnumeratorImpl(
                  CompositeDataSourceImpl* aCompositeDataSource,
                  nsIRDFResource* aSource,
                  nsIRDFResource* aProperty,
                  nsIRDFNode* aTarget,
                  PRBool aTruthValue,
                  PRBool aAllowNegativeAssertions,
                  PRBool aCoalesceDuplicateArcs)
    : CompositeEnumeratorImpl(aCompositeDataSource, aAllowNegativeAssertions, aCoalesceDuplicateArcs),
      mSource(aSource),
      mProperty(aProperty),
      mTarget(aTarget),
      mTruthValue(aTruthValue),
      mAllowNegativeAssertions(aAllowNegativeAssertions),
      mCoalesceDuplicateArcs(aCoalesceDuplicateArcs)
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

void
CompositeAssertionEnumeratorImpl::Destroy()
{
    // Keep the datasource alive for the duration of the stack
    // frame so its allocator stays valid.
    nsCOMPtr<nsIRDFCompositeDataSource> kungFuDeathGrip =
        dont_QueryInterface(mCompositeDataSource);

    nsFixedSizeAllocator& pool = mCompositeDataSource->mAllocator;
    this->~CompositeAssertionEnumeratorImpl();
    pool.Free(this, sizeof(*this));
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
	: mAllowNegativeAssertions(PR_TRUE),
	  mCoalesceDuplicateArcs(PR_TRUE),
      mUpdateBatchNest(0)
{
    NS_INIT_REFCNT();

    static const size_t kBucketSizes[] = {
        sizeof(CompositeAssertionEnumeratorImpl),
        sizeof(CompositeArcsInOutEnumeratorImpl) };

    static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);

    // Per news://news.mozilla.org/39BEC105.5090206%40netscape.com
    static const PRInt32 kInitialSize = 256;

    mAllocator.Init("nsCompositeDataSource", kBucketSizes, kNumBuckets, kInitialSize);

#ifdef PR_LOGGING
    if (nsRDFLog == nsnull) 
        nsRDFLog = PR_NewLogModule("RDF");
#endif
}

//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMPL_THREADSAFE_ADDREF(CompositeDataSourceImpl);

NS_IMETHODIMP_(nsrefcnt)
CompositeDataSourceImpl::Release()
{
    // We need a special implementation of Release() because the
    // composite datasource holds a reference to each datasource that
    // it "composes", and each database that the composite datasource
    // observes holds a reference _back_ to the composite datasource.
    NS_PRECONDITION(PRInt32(mRefCnt) > 0, "duplicate release");
    nsrefcnt count =
      PR_AtomicDecrement(NS_REINTERPRET_CAST(PRInt32 *, &mRefCnt));

    // When the number of references == the number of datasources,
    // then we know that all that remains are the circular
    // references from those datasources back to us. Release them.
    if (count == 0) {
        NS_LOG_RELEASE(this, count, "CompositeDataSourceImpl");
        mRefCnt = 1;
        NS_DELETEXPCOM(this);
        return 0;
    }
    else if (PRInt32(count) == mDataSources.Count()) {
        // We must add 1 here because otherwise the nested releases
        // on this object will enter this same code path.
        PR_AtomicIncrement(NS_REINTERPRET_CAST(PRInt32 *, &mRefCnt));
        
        PRInt32 dsCount;
        while (0 != (dsCount = mDataSources.Count())) {
            nsIRDFDataSource* ds =
              NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[dsCount-1]);
            mDataSources.RemoveElementAt(dsCount-1);
            ds->RemoveObserver(this);
            NS_RELEASE(ds);
        }
        // Nest into Release to deal with the one last reference we added above.
        // We don't want to assume that we can 'delete this' because an
        // extra reference might have been added by other code while we were 
        // calling out.
        NS_ASSERTION(mRefCnt >= 1, "bad mRefCnt");
        return Release();
    }
    else {
        NS_LOG_RELEASE(this, count, "CompositeDataSourceImpl");
        return count;
    }
}

NS_IMETHODIMP
CompositeDataSourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(NS_GET_IID(nsIRDFCompositeDataSource)) ||
        iid.Equals(NS_GET_IID(nsIRDFDataSource)) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
		NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(NS_GET_IID(nsIRDFObserver))) {
        *result = NS_STATIC_CAST(nsIRDFObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}



//----------------------------------------------------------------------
//
// nsIRDFDataSource interface
//

NS_IMETHODIMP
CompositeDataSourceImpl::GetURI(char* *uri)
{
    *uri = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetSource(nsIRDFResource* property,
                                   nsIRDFNode* target,
                                   PRBool tv,
                                   nsIRDFResource** source)
{
	if ((mAllowNegativeAssertions == PR_FALSE) && (tv == PR_FALSE))
		return(NS_RDF_NO_VALUE);

    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        nsresult rv;
        rv = ds->GetSource(property, target, tv, source);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_RDF_NO_VALUE)
            continue;

	if (mAllowNegativeAssertions == PR_FALSE)	return(NS_OK);

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

    if ((mAllowNegativeAssertions == PR_FALSE) && (aTruthValue == PR_FALSE))
        return(NS_RDF_NO_VALUE);

    *aResult = CompositeAssertionEnumeratorImpl::Create(mAllocator,
                                                        this, nsnull, aProperty,
                                                        aTarget, aTruthValue,
                                                        mAllowNegativeAssertions,
                                                        mCoalesceDuplicateArcs);

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

    if ((mAllowNegativeAssertions == PR_FALSE) && (aTruthValue == PR_FALSE))
        return(NS_RDF_NO_VALUE);

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

            if (mAllowNegativeAssertions == PR_TRUE) {
                if (HasAssertionN(count-1, aSource, aProperty, *aResult, !aTruthValue)) {
                    // whoops, it's been negated.
                    NS_RELEASE(*aResult);
                    return NS_RDF_NO_VALUE;
                }
            }
            return NS_OK;
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

    if ((mAllowNegativeAssertions == PR_FALSE) && (aTruthValue == PR_FALSE))
        return(NS_RDF_NO_VALUE);

    *aResult =
        CompositeAssertionEnumeratorImpl::Create(mAllocator, this,
                                                 aSource, aProperty, nsnull,
                                                 aTruthValue,
                                                 mAllowNegativeAssertions,
                                                 mCoalesceDuplicateArcs);

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

    if ((mAllowNegativeAssertions == PR_FALSE) && (aTruthValue == PR_FALSE))
        return(NS_RDF_ASSERTION_REJECTED);

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
CompositeDataSourceImpl::Change(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aOldTarget,
                                nsIRDFNode* aNewTarget)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aOldTarget != nsnull, "null ptr");
    if (! aOldTarget)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aNewTarget != nsnull, "null ptr");
    if (! aNewTarget)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // XXX So we're assuming that a datasource _must_ accept the
    // atomic change; i.e., we can't split it up across two
    // datasources. That sucks.

    // We iterate backwards from the last data source which was added
    // ("the most remote") to the first ("the most local"), trying to
    // apply the change in each.
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->Change(aSource, aProperty, aOldTarget, aNewTarget);
        if (NS_RDF_ASSERTION_ACCEPTED == rv)
            return rv;

        if (NS_FAILED(rv))
            return rv;
    }

    // nobody wanted to accept it
    return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Move(nsIRDFResource* aOldSource,
                              nsIRDFResource* aNewSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget)
{
    NS_PRECONDITION(aOldSource != nsnull, "null ptr");
    if (! aOldSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aNewSource != nsnull, "null ptr");
    if (! aNewSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // XXX So we're assuming that a datasource _must_ accept the
    // atomic move; i.e., we can't split it up across two
    // datasources. That sucks.

    // We iterate backwards from the last data source which was added
    // ("the most remote") to the first ("the most local"), trying to
    // apply the assertion in each.
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->Move(aOldSource, aNewSource, aProperty, aTarget);
        if (NS_RDF_ASSERTION_ACCEPTED == rv)
            return rv;

        if (NS_FAILED(rv))
            return rv;
    }

    // nobody wanted to accept it
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

    if ((mAllowNegativeAssertions == PR_FALSE) && (aTruthValue == PR_FALSE))
    {
        *aResult = PR_FALSE;
        return(NS_OK);
    }

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

	if (mAllowNegativeAssertions == PR_TRUE)
	{
	        PRBool hasNegation;
	        rv = datasource->HasAssertion(aSource, aProperty, aTarget, !aTruthValue, &hasNegation);
	        if (NS_FAILED(rv)) return rv;

	        if (hasNegation)
	        {
	            *aResult = PR_FALSE;
	            return NS_OK;
	        }
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

    // XXX ensure uniqueness?
    if (mObservers.AppendElement(aObserver))
        NS_ADDREF(aObserver);

    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::RemoveObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (mObservers.RemoveElement(aObserver))
        NS_RELEASE(aObserver);

    return NS_OK;
}

NS_IMETHODIMP 
CompositeDataSourceImpl::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    nsresult rv;
    *result = PR_FALSE;
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->HasArcIn(aNode, aArc, result);
        if (NS_FAILED(rv)) return rv;
        if (*result == PR_TRUE)
            return NS_OK;
    }
    return NS_OK;
}

NS_IMETHODIMP 
CompositeDataSourceImpl::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
    nsresult rv;
    *result = PR_FALSE;
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->HasArcOut(aSource, aArc, result);
        if (NS_FAILED(rv)) return rv;
        if (*result == PR_TRUE)
            return NS_OK;
    }
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
        CompositeArcsInOutEnumeratorImpl::Create(mAllocator, this, aTarget,
                                                 CompositeArcsInOutEnumeratorImpl::eArcsIn,
                                                 mAllowNegativeAssertions,
                                                 mCoalesceDuplicateArcs);

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
        CompositeArcsInOutEnumeratorImpl::Create(mAllocator, this, aSource,
                                                 CompositeArcsInOutEnumeratorImpl::eArcsOut,
                                                 mAllowNegativeAssertions,
                                                 mCoalesceDuplicateArcs);

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

#if 0
/*
 * shaver 19990824: until Composite is made to implement 
 * nsIRDFRemoteDataSource, we don't need or want this.  When it does implement
 * nsIRDFRemoteDataSource, we should do a similar thing with Refresh and
 * make a no-op Init.  I'll be back to finish that off later.
 */

NS_IMETHODIMP
CompositeDataSourceImpl::Flush()
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(ds);
        if (remote)
            remote->Flush();
    }
    return NS_OK;
}
#endif

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
CompositeDataSourceImpl::GetAllCmds(nsIRDFResource* source,
                                        nsISimpleEnumerator/*<nsIRDFResource>*/** result)
{
	nsCOMPtr<nsISupportsArray>	cmdArray;
	nsresult			rv;

	rv = NS_NewISupportsArray(getter_AddRefs(cmdArray));
	if (NS_FAILED(rv)) return(rv);

	for (PRInt32 i = 0; i < mDataSources.Count(); i++)
	{
		nsIRDFDataSource		*ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
		nsCOMPtr<nsISimpleEnumerator>	dsCmds;

		rv = ds->GetAllCmds(source, getter_AddRefs(dsCmds));
		if (NS_SUCCEEDED(rv))
		{
			PRBool	hasMore = PR_FALSE;
			while(NS_SUCCEEDED(rv = dsCmds->HasMoreElements(&hasMore)) && (hasMore == PR_TRUE))
			{
				nsCOMPtr<nsISupports>	item;
				if (NS_SUCCEEDED(rv = dsCmds->GetNext(getter_AddRefs(item))))
				{
					// rjc: do NOT strip out duplicate commands here
					// (due to items such as separators, it is done at a higher level)
					cmdArray->AppendElement(item);
				}
			}
			if (NS_FAILED(rv))	return(rv);
		}
	}
	nsISimpleEnumerator	*commands = new nsArrayEnumerator(cmdArray);
	if (! commands)
		return(NS_ERROR_OUT_OF_MEMORY);
	NS_ADDREF(commands);
	*result = commands;
	return(NS_OK);
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

        PRBool enabled = PR_TRUE;
        rv = ds->IsCommandEnabled(aSources, aCommand, aArguments, &enabled);
        if (NS_FAILED(rv) && (rv != NS_ERROR_NOT_IMPLEMENTED))
        {
        	return(rv);
        }

        if (! enabled) {
            *aResult = PR_FALSE;
            return(NS_OK);
        }
    }
    *aResult = PR_TRUE;
    return(NS_OK);
}

NS_IMETHODIMP
CompositeDataSourceImpl::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                   nsIRDFResource*   aCommand,
                                   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsresult rv = ds->DoCommand(aSources, aCommand, aArguments);
        if (NS_FAILED(rv) && (rv != NS_ERROR_NOT_IMPLEMENTED))
        {
		return(rv);   // all datasources must succeed
	}
    }
    return(NS_OK);
}

////////////////////////////////////////////////////////////////////////
// nsIRDFCompositeDataSource methods
// XXX rvg We should make this take an additional argument specifying where
// in the sequence of data sources (of the db), the new data source should
// fit in. Right now, the new datasource gets stuck at the end.
// need to add the observers of the CompositeDataSourceImpl to the new data source.

NS_IMETHODIMP
CompositeDataSourceImpl::GetAllowNegativeAssertions(PRBool *aAllowNegativeAssertions)
{
	*aAllowNegativeAssertions = mAllowNegativeAssertions;
	return(NS_OK);
}

NS_IMETHODIMP
CompositeDataSourceImpl::SetAllowNegativeAssertions(PRBool aAllowNegativeAssertions)
{
	mAllowNegativeAssertions = aAllowNegativeAssertions;
	return(NS_OK);
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetCoalesceDuplicateArcs(PRBool *aCoalesceDuplicateArcs)
{
	*aCoalesceDuplicateArcs = mCoalesceDuplicateArcs;
	return(NS_OK);
}

NS_IMETHODIMP
CompositeDataSourceImpl::SetCoalesceDuplicateArcs(PRBool aCoalesceDuplicateArcs)
{
	mCoalesceDuplicateArcs = aCoalesceDuplicateArcs;
	return(NS_OK);
}

NS_IMETHODIMP
CompositeDataSourceImpl::AddDataSource(nsIRDFDataSource* aDataSource)
{
    NS_ASSERTION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    mDataSources.AppendElement(aDataSource);
    aDataSource->AddObserver(this);
    NS_ADDREF(aDataSource);
    return NS_OK;
}



NS_IMETHODIMP
CompositeDataSourceImpl::RemoveDataSource(nsIRDFDataSource* aDataSource)
{
    NS_ASSERTION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;


    if (mDataSources.IndexOf(aDataSource) >= 0) {
        mDataSources.RemoveElement(aDataSource);
        aDataSource->RemoveObserver(this);
        NS_RELEASE(aDataSource);
    }
    return NS_OK;
}


NS_IMETHODIMP
CompositeDataSourceImpl::GetDataSources(nsISimpleEnumerator** _result)
{
    nsresult rv;

    // Create an nsISupportsArray, copy the datasources into it, then
    // use that to create an array enumerator.
    nsCOMPtr<nsISupportsArray> temp;
    rv = NS_NewISupportsArray(getter_AddRefs(temp));
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < mDataSources.Count(); ++i)
        temp->AppendElement(NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]));

    return NS_NewArrayEnumerator(_result, temp);
}

NS_IMETHODIMP
CompositeDataSourceImpl::OnAssert(nsIRDFDataSource* aDataSource,
                                  nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
    // Make sure that the assertion isn't masked by another
    // datasource.
    //
    // XXX We could make this more efficient if we knew _which_
    // datasource actually served up the OnAssert(): we could use
    // HasAssertionN() to only search datasources _before_ the
    // datasource that coughed up the assertion.
	nsresult	rv = NS_OK;

	if (mAllowNegativeAssertions == PR_TRUE)
	{   
		PRBool hasAssertion;
		rv = HasAssertion(aSource, aProperty, aTarget, PR_TRUE, &hasAssertion);
		if (NS_FAILED(rv)) return rv;

		if (! hasAssertion)
			return(NS_OK);
	}

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFObserver* obs = NS_STATIC_CAST(nsIRDFObserver*, mObservers[i]);
        obs->OnAssert(this, aSource, aProperty, aTarget);
        // XXX ignore return value?
    }
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::OnUnassert(nsIRDFDataSource* aDataSource,
                                    nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget)
{
    // Make sure that the un-assertion doesn't just unmask the
    // same assertion in a different datasource.
    //
    // XXX We could make this more efficient if we knew _which_
    // datasource actually served up the OnAssert(): we could use
    // HasAssertionN() to only search datasources _before_ the
    // datasource that coughed up the assertion.
    nsresult rv;

	if (mAllowNegativeAssertions == PR_TRUE)
	{   
		PRBool hasAssertion;
		rv = HasAssertion(aSource, aProperty, aTarget, PR_TRUE, &hasAssertion);
		if (NS_FAILED(rv)) return rv;

		if (hasAssertion)
			return NS_OK;
	}

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFObserver* obs = NS_STATIC_CAST(nsIRDFObserver*, mObservers[i]);
        obs->OnUnassert(this, aSource, aProperty, aTarget);
        // XXX ignore return value?
    }
    return NS_OK;
}


NS_IMETHODIMP
CompositeDataSourceImpl::OnChange(nsIRDFDataSource* aDataSource,
                                  nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aOldTarget,
                                  nsIRDFNode* aNewTarget)
{
    // Make sure that the change is actually visible, and not hidden
    // by an assertion in a different datasource.
    //
    // XXX Because of aggregation, this could actually mutate into a
    // variety of OnAssert or OnChange notifications, which we'll
    // ignore for now :-/.
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFObserver* obs = NS_STATIC_CAST(nsIRDFObserver*, mObservers[i]);
        obs->OnChange(this, aSource, aProperty, aOldTarget, aNewTarget);
        // XXX ignore return value?
    }
    return NS_OK;
}


NS_IMETHODIMP
CompositeDataSourceImpl::OnMove(nsIRDFDataSource* aDataSource,
                                nsIRDFResource* aOldSource,
                                nsIRDFResource* aNewSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget)
{
    // Make sure that the move is actually visible, and not hidden
    // by an assertion in a different datasource.
    //
    // XXX Because of aggregation, this could actually mutate into a
    // variety of OnAssert or OnMove notifications, which we'll
    // ignore for now :-/.
    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        nsIRDFObserver* obs = NS_STATIC_CAST(nsIRDFObserver*, mObservers[i]);
        obs->OnMove(this, aOldSource, aNewSource, aProperty, aTarget);
        // XXX ignore return value?
    }
    return NS_OK;
}


NS_IMETHODIMP
CompositeDataSourceImpl::BeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
    PRInt32 nest = mUpdateBatchNest++;
    if (nest == 0) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = NS_STATIC_CAST(nsIRDFObserver*, mObservers[i]);
            obs->BeginUpdateBatch(this);
            // XXX ignore return value?
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
CompositeDataSourceImpl::EndUpdateBatch(nsIRDFDataSource* aDataSource)
{
    PRInt32 nest = --mUpdateBatchNest;
    if (nest < 0) {
        NS_ERROR("badly nested update batch");
        mUpdateBatchNest = 0;
        return NS_ERROR_UNEXPECTED;
    }
    if (nest == 0) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = NS_STATIC_CAST(nsIRDFObserver*, mObservers[i]);
            obs->EndUpdateBatch(this);
            // XXX ignore return value?
        }
    }
    return NS_OK;
}
