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
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

/*

  Implementation for an in-memory RDF data store.

  TO DO

  1) Instrument this code to gather space and time performance
     characteristics.

  2) If/when RDF containers become commonplace, consider implementing
     a special case for them to improve access time to individual
     elements.

  3) Complete implementation of thread-safety; specifically, make
     assertions be reference counted objects (so that a cursor can
     still refer to an assertion that gets removed from the graph).

 */

#include "nsAgg.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIOutputStream.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFLiteral.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFPurgeableDataSource.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsAutoLock.h"
#include "nsEnumeratorUtils.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
#include "nsRDFBaseDataSources.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsFixedSizeAllocator.h"
#include "rdfutil.h"
#include "pldhash.h"
#include "plstr.h"
#include "prlog.h"
#include "rdf.h"

#if defined(MOZ_THREADSAFE_RDF)
#define NS_AUTOLOCK(_lock) nsAutoLock _autolock(_lock)
#else
#define NS_AUTOLOCK(_lock)
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog = nsnull;
#endif

// This struct is used as the slot value in the forward and reverse
// arcs hash tables.
//
// Assertion objects are reference counted, because each Assertion's
// ownership is shared between the datasource and any enumerators that
// are currently iterating over the datasource.
//
class Assertion 
{
public:
    static Assertion*
    Create(nsFixedSizeAllocator& aAllocator,
           nsIRDFResource* aSource,
           nsIRDFResource* aProperty,
           nsIRDFNode* aTarget,
           PRBool aTruthValue) {
        void* place = aAllocator.Alloc(sizeof(Assertion));
        return place
            ? ::new (place) Assertion(aSource, aProperty, aTarget, aTruthValue)
            : nsnull; }

    static void
    Destroy(nsFixedSizeAllocator& aAllocator,
            Assertion* aAssertion) {
        aAssertion->~Assertion();
        aAllocator.Free(aAssertion, sizeof(*aAssertion)); }

    Assertion(nsIRDFResource* aSource,
              nsIRDFResource* aProperty,
              nsIRDFNode* aTarget,
              PRBool aTruthValue);

    ~Assertion();

    void AddRef() { ++mRefCnt;  }

    void Release(nsFixedSizeAllocator& aAllocator) {
        if (--mRefCnt == 0)
            Destroy(aAllocator, this); }

    // public for now, because I'm too lazy to go thru and clean this up.
    nsIRDFResource* mSource;     // OWNER
    nsIRDFResource* mProperty;   // OWNER
    nsIRDFNode*     mTarget;     // OWNER
    Assertion*      mNext;
    Assertion*      mInvNext;
    PRPackedBool    mTruthValue;
    PRPackedBool    mMarked;
    PRInt16         mRefCnt;

    // For nsIRDFPurgeableDataSource
    void Mark() { mMarked = PR_TRUE; }
    PRBool IsMarked() { return mMarked; }
    void Unmark() { mMarked = PR_FALSE; }

private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) { return 0; }
    static void operator delete(void*, size_t) {}
};


Assertion::Assertion(nsIRDFResource* aSource,
                     nsIRDFResource* aProperty,
                     nsIRDFNode* aTarget,
                     PRBool aTruthValue)
    : mSource(aSource),
      mProperty(aProperty),
      mTarget(aTarget),
      mNext(nsnull),
      mInvNext(nsnull),
      mTruthValue(aTruthValue),
      mMarked(PR_FALSE),
      mRefCnt(0)
{
    MOZ_COUNT_CTOR(RDF_Assertion);

    NS_ADDREF(mSource);
    NS_ADDREF(mProperty);
    NS_ADDREF(mTarget);
}

Assertion::~Assertion()
{
    MOZ_COUNT_DTOR(RDF_Assertion);
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: Assertion\n", gInstanceCount);
#endif

    NS_RELEASE(mSource);
    NS_RELEASE(mProperty);
    NS_RELEASE(mTarget);
}


    
////////////////////////////////////////////////////////////////////////
// Utility routines

static inline PLHashNumber PR_CALLBACK
rdf_HashPointer(const void* key)
{
    return PLHashNumber(NS_PTR_TO_INT32(key)) >> 2;
}

////////////////////////////////////////////////////////////////////////
// InMemoryDataSource
class InMemoryArcsEnumeratorImpl;
class InMemoryAssertionEnumeratorImpl;
class InMemoryResourceEnumeratorImpl;

class InMemoryDataSource : public nsIRDFDataSource,
                           public nsIRDFPurgeableDataSource
{
protected:
    nsFixedSizeAllocator mAllocator;

    // These hash tables are keyed on pointers to nsIRDFResource
    // objects (the nsIRDFService ensures that there is only ever one
    // nsIRDFResource object per unique URI). The value of an entry is
    // an Assertion struct, which is a linked list of (subject
    // predicate object) triples.
    PLDHashTable mForwardArcs; 
    PLDHashTable mReverseArcs; 

    nsCOMPtr<nsISupportsArray> mObservers;  

    static const PRInt32 kInitialTableSize;

    static PLDHashOperator PR_CALLBACK
    DeleteForwardArcsEntry(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                           PRUint32 aNumber, void* aArg);

    static PLDHashOperator PR_CALLBACK
    ResourceEnumerator(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                       PRUint32 aNumber, void* aArg);

    friend class InMemoryArcsEnumeratorImpl;
    friend class InMemoryAssertionEnumeratorImpl;
    friend class InMemoryResourceEnumeratorImpl; // b/c it needs to enumerate mForwardArcs

    // Thread-safe writer implementation methods.
    nsresult
    LockedAssert(nsIRDFResource* source, 
                 nsIRDFResource* property, 
                 nsIRDFNode* target,
                 PRBool tv);

    nsresult
    LockedUnassert(nsIRDFResource* source,
                   nsIRDFResource* property,
                   nsIRDFNode* target);

    InMemoryDataSource(nsISupports* aOuter);
    virtual ~InMemoryDataSource();
    nsresult Init();

    friend NS_IMETHODIMP
    NS_NewRDFInMemoryDataSource(nsISupports* aOuter, const nsIID& aIID, void** aResult);

public:
    NS_DECL_AGGREGATED

    // nsIRDFDataSource methods
    NS_DECL_NSIRDFDATASOURCE

    // nsIRDFPurgeableDataSource methods
    NS_DECL_NSIRDFPURGEABLEDATASOURCE

protected:
    static PLDHashOperator PR_CALLBACK
    SweepForwardArcsEntries(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                            PRUint32 aNumber, void* aArg);

    struct Entry {
        PLDHashEntryHdr mHdr;
        nsIRDFNode*     mNode;
        Assertion*      mAssertions;
    };

public:
    // Implementation methods
    Assertion*
    GetForwardArcs(nsIRDFResource* u) {
        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mForwardArcs, u, PL_DHASH_LOOKUP);
        return PL_DHASH_ENTRY_IS_BUSY(hdr)
            ? NS_REINTERPRET_CAST(Entry*, hdr)->mAssertions
            : nsnull; }

    Assertion*
    GetReverseArcs(nsIRDFNode* v) {
        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mReverseArcs, v, PL_DHASH_LOOKUP);
        return PL_DHASH_ENTRY_IS_BUSY(hdr)
            ? NS_REINTERPRET_CAST(Entry*, hdr)->mAssertions
            : nsnull; }

    void
    SetForwardArcs(nsIRDFResource* u, Assertion* as) {
        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mForwardArcs, u,
                                                    as ? PL_DHASH_ADD : PL_DHASH_REMOVE);
        if (as && hdr) {
            Entry* entry = NS_REINTERPRET_CAST(Entry*, hdr);
            entry->mNode = u;
            entry->mAssertions = as;
        } }

    void
    SetReverseArcs(nsIRDFNode* v, Assertion* as) {
        PLDHashEntryHdr* hdr = PL_DHashTableOperate(&mReverseArcs, v,
                                                    as ? PL_DHASH_ADD : PL_DHASH_REMOVE);
        if (as && hdr) {
            Entry* entry = NS_REINTERPRET_CAST(Entry*, hdr);
            entry->mNode = v;
            entry->mAssertions = as;
        } }

#ifdef PR_LOGGING
    void
    LogOperation(const char* aOperation,
                 nsIRDFResource* asource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 PRBool aTruthValue = PR_TRUE);
#endif

#ifdef MOZ_THREADSAFE_RDF
    // This datasource's monitor object.
    PRLock* mLock;
#endif
};

//----------------------------------------------------------------------
//
// InMemoryAssertionEnumeratorImpl
//

/**
 * InMemoryAssertionEnumeratorImpl
 */
class InMemoryAssertionEnumeratorImpl : public nsISimpleEnumerator
{
private:
    InMemoryDataSource* mDataSource;
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    nsIRDFNode*     mValue;
    PRInt32         mCount;
    PRBool          mTruthValue;
    Assertion*      mNextAssertion;

    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) { return 0; }
    static void operator delete(void*, size_t) {}

    InMemoryAssertionEnumeratorImpl(InMemoryDataSource* aDataSource,
                                    nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue);

    virtual ~InMemoryAssertionEnumeratorImpl();

public:
    static InMemoryAssertionEnumeratorImpl*
    Create(InMemoryDataSource* aDataSource,
           nsIRDFResource* aSource,
           nsIRDFResource* aProperty,
           nsIRDFNode* aTarget,
           PRBool aTruthValue) {
        void* place = aDataSource->mAllocator.Alloc(sizeof(InMemoryAssertionEnumeratorImpl));
        return place
            ? ::new (place) InMemoryAssertionEnumeratorImpl(aDataSource,
                                                            aSource, aProperty, aTarget,
                                                            aTruthValue)
            : nsnull; }

    static void
    Destroy(InMemoryAssertionEnumeratorImpl* aEnumerator) {
        // Keep the datasource alive for the duration of the stack
        // frame so its allocator stays valid.
        nsCOMPtr<nsIRDFDataSource> kungFuDeathGrip =
            dont_QueryInterface(aEnumerator->mDataSource);

        // Grab the pool from the datasource; since we keep the
        // datasource alive, this has to be safe.
        nsFixedSizeAllocator& pool = aEnumerator->mDataSource->mAllocator;
        aEnumerator->~InMemoryAssertionEnumeratorImpl();
        pool.Free(aEnumerator, sizeof(*aEnumerator)); }

    // nsISupports interface
    NS_DECL_ISUPPORTS
   
    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR
};

////////////////////////////////////////////////////////////////////////


InMemoryAssertionEnumeratorImpl::InMemoryAssertionEnumeratorImpl(
                 InMemoryDataSource* aDataSource,
                 nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 PRBool aTruthValue)
    : mDataSource(aDataSource),
      mSource(aSource),
      mProperty(aProperty),
      mTarget(aTarget),
      mValue(nsnull),
      mCount(0),
      mTruthValue(aTruthValue),
      mNextAssertion(nsnull)
{
    NS_INIT_REFCNT();

    NS_ADDREF(mDataSource);
    NS_IF_ADDREF(mSource);
    NS_ADDREF(mProperty);
    NS_IF_ADDREF(mTarget);

    if (mSource) {
        mNextAssertion = mDataSource->GetForwardArcs(mSource);
    }
    else {
        mNextAssertion = mDataSource->GetReverseArcs(mTarget);
    }

    // Add an owning reference from the enumerator
    if (mNextAssertion)
        mNextAssertion->AddRef();
}

InMemoryAssertionEnumeratorImpl::~InMemoryAssertionEnumeratorImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: InMemoryAssertionEnumeratorImpl\n", gInstanceCount);
#endif

    if (mNextAssertion)
        mNextAssertion->Release(mDataSource->mAllocator);

    NS_IF_RELEASE(mDataSource);
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mProperty);
    NS_IF_RELEASE(mTarget);
    NS_IF_RELEASE(mValue);
}

NS_IMPL_ADDREF(InMemoryAssertionEnumeratorImpl)
NS_IMPL_RELEASE_WITH_DESTROY(InMemoryAssertionEnumeratorImpl, Destroy(this))
NS_IMPL_QUERY_INTERFACE1(InMemoryAssertionEnumeratorImpl, nsISimpleEnumerator)

NS_IMETHODIMP
InMemoryAssertionEnumeratorImpl::HasMoreElements(PRBool* aResult)
{
    NS_AUTOLOCK(mDataSource->mLock);

    if (mValue) {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    while (mNextAssertion) {
        PRBool foundIt = PR_FALSE;
        if ((mProperty == mNextAssertion->mProperty) && (mTruthValue == mNextAssertion->mTruthValue)) {
            if (mSource) {
                mValue = mNextAssertion->mTarget;
                NS_ADDREF(mValue);
            }
            else {
                mValue = mNextAssertion->mSource;
                NS_ADDREF(mValue);
            }
            foundIt = PR_TRUE;
        }

        // Remember the last assertion we were holding on to
        Assertion* as = mNextAssertion;

        // iterate
        mNextAssertion = (mSource) ? mNextAssertion->mNext : mNextAssertion->mInvNext;

        // grab an owning reference from the enumerator to the next assertion
        if (mNextAssertion)
            mNextAssertion->AddRef();

        // ...and release the reference from the enumerator to the old one.
        as->Release(mDataSource->mAllocator);

        if (foundIt) {
            *aResult = PR_TRUE;
            return NS_OK;
        }
    }

    *aResult = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
InMemoryAssertionEnumeratorImpl::GetNext(nsISupports** aResult)
{
    nsresult rv;

    PRBool hasMore;
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore)
        return NS_ERROR_UNEXPECTED;

    // Don't AddRef: we "transfer" ownership to the caller
    *aResult = mValue;
    mValue = nsnull;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//

/**
 * This class is a little bit bizarre in that it implements both the
 * <tt>nsIRDFArcsOutCursor</tt> and <tt>nsIRDFArcsInCursor</tt> interfaces.
 * Because the structure of the in-memory graph is pretty flexible, it's
 * fairly easy to parameterize this class. The only funky thing to watch
 * out for is the mutliple inheiritance clashes.
 */

class InMemoryArcsEnumeratorImpl : public nsISimpleEnumerator
{
private:
    // Hide so that only Create() and Destroy() can be used to
    // allocate and deallocate from the heap
    static void* operator new(size_t) { return 0; }
    static void operator delete(void*, size_t) {}

    InMemoryDataSource* mDataSource;
    nsIRDFResource*     mSource;
    nsIRDFNode*         mTarget;
    nsAutoVoidArray     mAlreadyReturned;
    nsIRDFResource*     mCurrent;
    Assertion*          mAssertion;

    InMemoryArcsEnumeratorImpl(InMemoryDataSource* aDataSource,
                               nsIRDFResource* aSource,
                               nsIRDFNode* aTarget);

    virtual ~InMemoryArcsEnumeratorImpl();

public:
    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    static InMemoryArcsEnumeratorImpl*
    Create(InMemoryDataSource* aDataSource,
           nsIRDFResource* aSource,
           nsIRDFNode* aTarget) {
        void* place = aDataSource->mAllocator.Alloc(sizeof(InMemoryArcsEnumeratorImpl));
        return place
            ? ::new (place) InMemoryArcsEnumeratorImpl(aDataSource, aSource, aTarget)
            : nsnull; }

    static void
    Destroy(InMemoryArcsEnumeratorImpl* aEnumerator) {
        // Keep the datasource alive for the duration of the stack
        // frame so its allocator stays valid.
        nsCOMPtr<nsIRDFDataSource> kungFuDeathGrip =
            dont_QueryInterface(aEnumerator->mDataSource);

        // Grab the pool from the datasource; since we keep the
        // datasource alive, this has to be safe.
        nsFixedSizeAllocator& pool = aEnumerator->mDataSource->mAllocator;
        aEnumerator->~InMemoryArcsEnumeratorImpl();
        pool.Free(aEnumerator, sizeof(*aEnumerator)); }
};


InMemoryArcsEnumeratorImpl::InMemoryArcsEnumeratorImpl(InMemoryDataSource* aDataSource,
                                                       nsIRDFResource* aSource,
                                                       nsIRDFNode* aTarget)
    : mDataSource(aDataSource),
      mSource(aSource),
      mTarget(aTarget),
      mCurrent(nsnull)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDataSource);
    NS_IF_ADDREF(mSource);
    NS_IF_ADDREF(mTarget);

    if (mSource) {
        // cast okay because it's a closed system
        mAssertion = mDataSource->GetForwardArcs(mSource);
    }
    else {
        mAssertion = mDataSource->GetReverseArcs(mTarget);
    }
}

InMemoryArcsEnumeratorImpl::~InMemoryArcsEnumeratorImpl()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: InMemoryArcsEnumeratorImpl\n", gInstanceCount);
#endif

    NS_RELEASE(mDataSource);
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mTarget);
    NS_IF_RELEASE(mCurrent);

    for (PRInt32 i = mAlreadyReturned.Count() - 1; i >= 0; --i) {
        nsIRDFResource* resource = (nsIRDFResource*) mAlreadyReturned[i];
        NS_RELEASE(resource);
    }
}

NS_IMPL_ADDREF(InMemoryArcsEnumeratorImpl)
NS_IMPL_RELEASE_WITH_DESTROY(InMemoryArcsEnumeratorImpl, Destroy(this))
NS_IMPL_QUERY_INTERFACE1(InMemoryArcsEnumeratorImpl, nsISimpleEnumerator)

NS_IMETHODIMP
InMemoryArcsEnumeratorImpl::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (mCurrent) {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    NS_AUTOLOCK(mDataSource->mLock);
    while (mAssertion) {
        nsIRDFResource* next = mAssertion->mProperty;

        PRBool alreadyReturned = PR_FALSE;
        for (PRInt32 i = mAlreadyReturned.Count() - 1; i >= 0; --i) {
            if (mAlreadyReturned[i] == mCurrent) {
                alreadyReturned = PR_TRUE;
                break;
            }
        }

        mAssertion = (mSource ? mAssertion->mNext : mAssertion->mInvNext);

        if (! alreadyReturned) {
            mCurrent = next;
            NS_ADDREF(mCurrent);
            *aResult = PR_TRUE;
            return NS_OK;
        }
    }

    *aResult = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
InMemoryArcsEnumeratorImpl::GetNext(nsISupports** aResult)
{
    nsresult rv;

    PRBool hasMore;
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore)
        return NS_ERROR_UNEXPECTED;

    // Add this to the set of things we've already returned so that we
    // can ensure uniqueness
    NS_ADDREF(mCurrent);
    mAlreadyReturned.AppendElement(mCurrent);

    // Don't AddRef: we "transfer" ownership to the caller
    *aResult = mCurrent;
    mCurrent = nsnull;

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// InMemoryDataSource

NS_IMETHODIMP
NS_NewRDFInMemoryDataSource(nsISupports* aOuter, const nsIID& aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter && !aIID.Equals(NS_GET_IID(nsISupports))) {
        NS_ERROR("aggregation requires nsISupports");
        return NS_ERROR_ILLEGAL_VALUE;
    }

    InMemoryDataSource* datasource = new InMemoryDataSource(aOuter);
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    rv = datasource->Init();
    if (NS_SUCCEEDED(rv)) {
        datasource->fAggregated.AddRef();
        rv = datasource->AggregatedQueryInterface(aIID, aResult); // This'll AddRef()
        datasource->fAggregated.Release();

        if (NS_SUCCEEDED(rv))
            return rv;
    }

    delete datasource;
    *aResult = nsnull;
    return rv;
}


InMemoryDataSource::InMemoryDataSource(nsISupports* aOuter)
{
    NS_INIT_AGGREGATED(aOuter);

    static const size_t kBucketSizes[] = {
        sizeof(Assertion),
        sizeof(PLHashEntry),
        sizeof(InMemoryArcsEnumeratorImpl),
        sizeof(InMemoryAssertionEnumeratorImpl) };

    static const PRInt32 kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);

    // Per news://news.mozilla.org/39BEC105.5090206%40netscape.com
    static const PRInt32 kInitialSize = 1024;

    mAllocator.Init("nsInMemoryDataSource", kBucketSizes, kNumBuckets, kInitialSize);

#ifdef MOZ_THREADSAFE_RDF
    mLock = nsnull;
#endif
}


nsresult
InMemoryDataSource::Init()
{
    PL_DHashTableInit(&mForwardArcs,
                      PL_DHashGetStubOps(),
                      nsnull,
                      sizeof(Entry),
                      PL_DHASH_MIN_SIZE);

    PL_DHashTableInit(&mReverseArcs,
                      PL_DHashGetStubOps(),
                      nsnull,
                      sizeof(Entry),
                      PL_DHASH_MIN_SIZE);

#ifdef MOZ_THREADSAFE_RDF
    mLock = PR_NewLock();
    if (! mLock)
        return NS_ERROR_OUT_OF_MEMORY;
#endif

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("InMemoryDataSource");
#endif

    return NS_OK;
}


InMemoryDataSource::~InMemoryDataSource()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: InMemoryDataSource\n", gInstanceCount);
#endif

    // This'll release all of the Assertion objects that are
    // associated with this data source. We only need to do this
    // for the forward arcs, because the reverse arcs table
    // indexes the exact same set of resources.
    PL_DHashTableEnumerate(&mForwardArcs, DeleteForwardArcsEntry, &mAllocator);
    PL_DHashTableFinish(&mForwardArcs);

    PL_DHashTableFinish(&mReverseArcs);

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("InMemoryDataSource(%p): destroyed.", this));

#ifdef MOZ_THREADSAFE_RDF
    PR_DestroyLock(mLock);
#endif
}

PLDHashOperator PR_CALLBACK
InMemoryDataSource::DeleteForwardArcsEntry(PLDHashTable* aTable, PLDHashEntryHdr* aHdr,
                                           PRUint32 aNumber, void* aArg)
{
    Entry* entry = NS_REINTERPRET_CAST(Entry*, aHdr);
    nsFixedSizeAllocator* allocator = NS_STATIC_CAST(nsFixedSizeAllocator*, aArg);

    Assertion* as = entry->mAssertions;
    while (as) {
        Assertion* doomed = as;
        as = as->mNext;

        // Unlink, and release the datasource's reference.
        doomed->mNext = doomed->mInvNext = nsnull;
        doomed->Release(*allocator);
    }
    return PL_DHASH_NEXT;
}


////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(InMemoryDataSource)

NS_IMETHODIMP
InMemoryDataSource::AggregatedQueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsISupports*, &fAggregated);
    }
    else if (aIID.Equals(NS_GET_IID(nsIRDFDataSource))) {
        *aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsIRDFPurgeableDataSource))) {
        *aResult = NS_STATIC_CAST(nsIRDFPurgeableDataSource*, this);
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }

    NS_ADDREF(NS_STATIC_CAST(nsISupports*, *aResult));
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////


#ifdef PR_LOGGING
void
InMemoryDataSource::LogOperation(const char* aOperation,
                                 nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget,
                                 PRBool aTruthValue)
{
    if (! PR_LOG_TEST(gLog, PR_LOG_ALWAYS))
        return;

    nsXPIDLCString uri;
    aSource->GetValue(getter_Copies(uri));
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("InMemoryDataSource(%p): %s", this, aOperation));

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("  [(%p)%s]--", aSource, (const char*) uri));

    aProperty->GetValue(getter_Copies(uri));

    char tv = (aTruthValue ? '-' : '!');
    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("  --%c[(%p)%s]--", tv, aProperty, (const char*) uri));

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if ((resource = do_QueryInterface(aTarget)) != nsnull) {
        resource->GetValue(getter_Copies(uri));
        PR_LOG(gLog, PR_LOG_ALWAYS,
           ("  -->[(%p)%s]", aTarget, (const char*) uri));
    }
    else if ((literal = do_QueryInterface(aTarget)) != nsnull) {
        nsXPIDLString value;
        literal->GetValue(getter_Copies(value));
        nsAutoString valueStr(value);
        char* valueCStr = ToNewCString(valueStr);

        PR_LOG(gLog, PR_LOG_ALWAYS,
           ("  -->(\"%s\")\n", valueCStr));

        nsCRT::free(valueCStr);
    }
    else {
        PR_LOG(gLog, PR_LOG_ALWAYS,
           ("  -->(unknown-type)\n"));
    }
}
#endif


NS_IMETHODIMP
InMemoryDataSource::GetURI(char* *uri)
{
    NS_PRECONDITION(uri != nsnull, "null ptr");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    *uri = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetSource(nsIRDFResource* property,
                              nsIRDFNode* target,
                              PRBool tv,
                              nsIRDFResource** source)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nsnull, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    for (Assertion* as = GetReverseArcs(target); as != nsnull; as = as->mNext) {
        if ((property == as->mProperty) && (as->mTruthValue == tv)) {
            *source = as->mSource;
            NS_ADDREF(as->mSource);
            return NS_OK;
        }
    }
    *source = nsnull;
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
InMemoryDataSource::GetTarget(nsIRDFResource* source,
                              nsIRDFResource* property,
                              PRBool tv,
                              nsIRDFNode** target)
{
    NS_PRECONDITION(source != nsnull, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(property != nsnull, "null ptr");
    if (! property)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(target != nsnull, "null ptr");
    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    for (Assertion* as = GetForwardArcs(source); as != nsnull; as = as->mNext) {
        if ((property == as->mProperty) && (as->mTruthValue == tv)) {
            *target = as->mTarget;
            NS_ADDREF(as->mTarget);
            return NS_OK;
        }
    }

    // If we get here, then there was no target with for the specified
    // property & truth value.
    *target = nsnull;
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
InMemoryDataSource::HasAssertion(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 nsIRDFNode* target,
                                 PRBool tv,
                                 PRBool* hasAssertion)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    if (! property)
        return NS_ERROR_NULL_POINTER;

    if (! target)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    for (Assertion* as = GetForwardArcs(source); as != nsnull; as = as->mNext) {
        if (property != as->mProperty)
            continue;

        if (target != as->mTarget)
            continue;

        if (as->mTruthValue != tv)
            continue;

        // found it!
        *hasAssertion = PR_TRUE;
        return NS_OK;
    }

    // If we get here, we couldn't find the assertion
    *hasAssertion = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetSources(nsIRDFResource* aProperty,
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

    NS_AUTOLOCK(mLock);

    InMemoryAssertionEnumeratorImpl* result =
        InMemoryAssertionEnumeratorImpl::Create(this, nsnull, aProperty,
                                                  aTarget, aTruthValue);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetTargets(nsIRDFResource* aSource,
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

    NS_AUTOLOCK(mLock);

    InMemoryAssertionEnumeratorImpl* result =
        InMemoryAssertionEnumeratorImpl::Create(this, aSource, aProperty,
                                                nsnull, aTruthValue);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}


nsresult
InMemoryDataSource::LockedAssert(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget,
                                 PRBool aTruthValue)
{
#ifdef PR_LOGGING
    LogOperation("ASSERT", aSource, aProperty, aTarget, aTruthValue);
#endif

    Assertion* next = GetForwardArcs(aSource);
    Assertion* prev = next;
    Assertion* as = nsnull;

    // Walk to the end of the linked list.
    // XXX shouldn't we just keep a pointer to the end, or insert at the front???
    while (next) {
        if (aProperty == next->mProperty) {
            if (aTarget == next->mTarget) {
                // Wow, we already had the assertion. Make sure that the
                // truth values are correct and bail.
                next->mTruthValue = aTruthValue;
                return NS_OK;
            }
        }

        prev = next;
        next = next->mNext;
    }

    as = Assertion::Create(mAllocator, aSource, aProperty, aTarget, aTruthValue);
    if (! as)
        return NS_ERROR_OUT_OF_MEMORY;

    // Link it in to the "forward arcs" table
    if (!prev) {
        SetForwardArcs(aSource, as);
    } else {
        prev->mNext = as;
    }

    // Link it in to the "reverse arcs" table

    // XXX Shouldn't we keep a pointer to the end of the list to make
    // sure this is O(1)?
    prev = nsnull;
    next = GetReverseArcs(aTarget);
    while (next) {
        prev = next;
        next = next->mInvNext;
    }

    if (!prev) {
        SetReverseArcs(aTarget, as);
    } else {
        prev->mInvNext = as;
    }

    // Add the datasource's owning reference.
    as->AddRef();

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Assert(nsIRDFResource* aSource,
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

    {
        NS_AUTOLOCK(mLock);
        rv = LockedAssert(aSource, aProperty, aTarget, aTruthValue);
        if (NS_FAILED(rv)) return rv;
    }

    // notify observers
    if (mObservers) {
        PRUint32 count;
        rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnAssert(this, aSource, aProperty, aTarget);
            NS_RELEASE(obs);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


nsresult
InMemoryDataSource::LockedUnassert(nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   nsIRDFNode* aTarget)
{
#ifdef PR_LOGGING
    LogOperation("UNASSERT", aSource, aProperty, aTarget);
#endif

    Assertion* next = GetForwardArcs(aSource);
    Assertion* prev = next;
    Assertion* as = nsnull;

    while (next) {
        if (aProperty == next->mProperty) {
            if (aTarget == next->mTarget) {
                if (prev == next) {
                    SetForwardArcs(aSource, next->mNext);
                } else {
                    prev->mNext = next->mNext;
                }
                as = next;
                break;
            }
        }

        prev = next;
        next = next->mNext;
    }

    // We don't even have the assertion, so just bail.
    if (!as)
        return NS_OK;

#ifdef DEBUG
    PRBool foundReverseArc = PR_FALSE;
#endif

    next = prev = GetReverseArcs(aTarget);
    while (next) {
        if (next == as) {
            if (prev == next) {
                SetReverseArcs(aTarget, next->mInvNext);
            } else {
                prev->mInvNext = next->mInvNext;
            }
#ifdef DEBUG
            foundReverseArc = PR_TRUE;
#endif
            break;
        }
        prev = next;
        next = next->mInvNext;
    }

#ifdef DEBUG
    NS_ASSERTION(foundReverseArc, "in-memory db corrupted: unable to find reverse arc");
#endif

    // Unlink, and release the datasource's reference
    as->mNext = as->mInvNext = nsnull;
    as->Release(mAllocator);

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Unassert(nsIRDFResource* aSource,
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

    {
        NS_AUTOLOCK(mLock);

        rv = LockedUnassert(aSource, aProperty, aTarget);
        if (NS_FAILED(rv)) return rv;
    }

    // Notify the world
    if (mObservers) {
        PRUint32 count;
        rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(this, aSource, aProperty, aTarget);
            NS_RELEASE(obs);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


NS_IMETHODIMP
InMemoryDataSource::Change(nsIRDFResource* aSource,
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

    {
        NS_AUTOLOCK(mLock);

        // XXX We can implement LockedChange() if we decide that this
        // is a performance bottleneck.

        rv = LockedUnassert(aSource, aProperty, aOldTarget);
        if (NS_FAILED(rv)) return rv;

        rv = LockedAssert(aSource, aProperty, aNewTarget, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    // Notify the world
    if (mObservers) {
        PRUint32 count;
        rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnChange(this, aSource, aProperty, aOldTarget, aNewTarget);
            NS_RELEASE(obs);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


NS_IMETHODIMP
InMemoryDataSource::Move(nsIRDFResource* aOldSource,
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

    {
        NS_AUTOLOCK(mLock);

        // XXX We can implement LockedMove() if we decide that this
        // is a performance bottleneck.

        rv = LockedUnassert(aOldSource, aProperty, aTarget);
        if (NS_FAILED(rv)) return rv;

        rv = LockedAssert(aNewSource, aProperty, aTarget, PR_TRUE);
        if (NS_FAILED(rv)) return rv;
    }

    // Notify the world
    if (mObservers) {
        PRUint32 count;
        rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnMove(this, aOldSource, aNewSource, aProperty, aTarget);
            NS_RELEASE(obs);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


NS_IMETHODIMP
InMemoryDataSource::AddObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    if (! mObservers) {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }

    mObservers->AppendElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::RemoveObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    if (! mObservers)
        return NS_OK;

    mObservers->RemoveElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP 
InMemoryDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    NS_AUTOLOCK(mDataSource->mLock);

    Assertion* ass = GetReverseArcs(aNode);
    while (ass) {
        nsIRDFResource* elbow = ass->mProperty;
        if (elbow == aArc) {
            *result = PR_TRUE;
            return NS_OK;
        }
        ass = ass->mInvNext;
    }
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP 
InMemoryDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
    NS_AUTOLOCK(mDataSource->mLock);

    Assertion* ass = GetForwardArcs(aSource);
    while (ass) {
        nsIRDFResource* elbow = ass->mProperty;
        if (elbow == aArc) {
            *result = PR_TRUE;
            return NS_OK;
        }
        ass = ass->mNext;
    }
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::ArcLabelsIn(nsIRDFNode* aTarget, nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    InMemoryArcsEnumeratorImpl* result =
        InMemoryArcsEnumeratorImpl::Create(this, nsnull, aTarget);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::ArcLabelsOut(nsIRDFResource* aSource, nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    InMemoryArcsEnumeratorImpl* result =
        InMemoryArcsEnumeratorImpl::Create(this, aSource, nsnull);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}

PLDHashOperator PR_CALLBACK
InMemoryDataSource::ResourceEnumerator(PLDHashTable* aTable,
                                       PLDHashEntryHdr* aHdr,
                                       PRUint32 aNumber, void* aArg)
{
    Entry* entry = NS_REINTERPRET_CAST(Entry*, aHdr);
    nsISupportsArray* resources = NS_STATIC_CAST(nsISupportsArray*, aArg);

    resources->AppendElement(entry->mNode);
    return PL_DHASH_NEXT;
}


NS_IMETHODIMP
InMemoryDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
    nsresult rv;

    nsCOMPtr<nsISupportsArray> values;
    rv = NS_NewISupportsArray(getter_AddRefs(values));
    if (NS_FAILED(rv)) return rv;

    NS_AUTOLOCK(mLock);

    // Enumerate all of our entries into an nsISupportsArray.
    PL_DHashTableEnumerate(&mForwardArcs, ResourceEnumerator, values.get());

    *aResult = new nsArrayEnumerator(values);
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetAllCommands(nsIRDFResource* source,
                                   nsIEnumerator/*<nsIRDFResource>*/** commands)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InMemoryDataSource::GetAllCmds(nsIRDFResource* source,
                               nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	return(NS_NewEmptyEnumerator(commands));
}

NS_IMETHODIMP
InMemoryDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                     nsIRDFResource*   aCommand,
                                     nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                     PRBool* aResult)
{
    *aResult = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                              nsIRDFResource*   aCommand,
                              nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFPurgeableDataSource methods

NS_IMETHODIMP
InMemoryDataSource::Mark(nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget,
                         PRBool aTruthValue,
                         PRBool* aDidMark)
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

    NS_AUTOLOCK(mLock);

    for (Assertion* as = GetForwardArcs(aSource); as != nsnull; as = as->mNext) {
        if (aProperty != as->mProperty)
            continue;

        if (aTarget != as->mTarget)
            continue;

        if (as->mTruthValue != aTruthValue)
            continue;

        // found it! so mark it.
        as->Mark();
        *aDidMark = PR_TRUE;

#ifdef PR_LOGGING
        LogOperation("MARK", aSource, aProperty, aTarget, aTruthValue);
#endif

        return NS_OK;
    }

    // If we get here, we couldn't find the assertion
    *aDidMark = PR_FALSE;
    return NS_OK;
}


struct SweepInfo {
    Assertion* mUnassertList;
    PLDHashTable* mReverseArcs;
};

NS_IMETHODIMP
InMemoryDataSource::Sweep()
{
    SweepInfo info = { nsnull, &mReverseArcs };

    {
        // Remove all the assertions while holding the lock, but don't notify anyone.
        NS_AUTOLOCK(mLock);
        PL_DHashTableEnumerate(&mForwardArcs, SweepForwardArcsEntries, &info);
    }

    // Now we've left the autolock. Do the notification.
    Assertion* as = info.mUnassertList;
    while (as) {
#ifdef PR_LOGGING
        LogOperation("SWEEP", as->mSource, as->mProperty, as->mTarget, as->mTruthValue);
#endif
        
        if (mObservers) {
            nsresult rv;
            PRUint32 count;
            rv = mObservers->Count(&count);
            if (NS_SUCCEEDED(rv)) {
                for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
                    nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
                    obs->OnUnassert(this, as->mSource, as->mProperty, as->mTarget);
                    // XXX ignore return value?
                }
            }
        }

        Assertion* doomed = as;
        as = as->mNext;

        // Unlink, and release the datasource's reference
        doomed->mNext = doomed->mInvNext = nsnull;
        doomed->Release(mAllocator);
    }

    return NS_OK;
}


PLDHashOperator PR_CALLBACK
InMemoryDataSource::SweepForwardArcsEntries(PLDHashTable* aTable,
                                            PLDHashEntryHdr* aHdr,
                                            PRUint32 aNumber, void* aArg)
{
    Entry* entry = NS_REINTERPRET_CAST(Entry*, aHdr);
    SweepInfo* info = NS_STATIC_CAST(SweepInfo*, aArg);

    Assertion* as = entry->mAssertions;
    Assertion* prev = nsnull;
    while (as) {
        if (as->IsMarked()) {
            prev = as;
            as->Unmark();
            as = as->mNext;
        }
        else {
            // remove from the list of assertions in the datasource
            Assertion* next = as->mNext;
            if (prev) {
                prev->mNext = next;
            }
            else {
                // it's the first one. update the hashtable entry.
                entry->mAssertions = next;
            }

            // remove from the reverse arcs
            PLDHashEntryHdr* hdr =
                PL_DHashTableOperate(info->mReverseArcs, as->mTarget, PL_DHASH_LOOKUP);

            NS_ASSERTION(PL_DHASH_ENTRY_IS_BUSY(hdr), "no assertion in reverse arcs");

            Entry* rentry = NS_REINTERPRET_CAST(Entry*, hdr);
            Assertion* ras = rentry->mAssertions;
            Assertion* rprev = nsnull;
            while (ras) {
                if (ras == as) {
                    if (rprev) {
                        rprev->mInvNext = ras->mInvNext;
                    }
                    else {
                        // it's the first one. update the hashtable entry.
                        rentry->mAssertions = ras->mInvNext;
                    }
                    as->mInvNext = nsnull; // for my sanity.
                    break;
                }
                rprev = ras;
                ras = ras->mInvNext;
            }

            // Wow, it was the _only_ one. Unhash it.
            if (! rentry->mAssertions)
                PL_DHashTableOperate(info->mReverseArcs, as->mTarget, PL_DHASH_REMOVE);
            
            // add to the list of assertions to unassert
            as->mNext = info->mUnassertList;
            info->mUnassertList = as;

            // Advance to the next assertion
            as = next;
        }
    }

    PLDHashOperator result = PL_DHASH_NEXT;

    // if no more assertions exist for this resource, then unhash it.
    if (! entry->mAssertions)
        result = PL_DHASH_REMOVE;

    return result;
}

////////////////////////////////////////////////////////////////////////

