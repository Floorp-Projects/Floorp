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
#include "nsXPIDLString.h"
#include "rdfutil.h"
#include "plhash.h"
#include "plstr.h"
#include "prlog.h"
#include "rdf.h"

#if 1 // defined(MOZ_THREADSAFE_RDF)
#define NS_AUTOLOCK(__monitor) nsAutoLock __lock(__monitor)
#else
#define NS_AUTOLOCK(__monitor)
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog = nsnull;
#endif

// This struct is used as the slot value in the forward and reverse
// arcs hash tables.
class Assertion 
{
public:
    Assertion(nsIRDFResource* aSource,
              nsIRDFResource* aProperty,
              nsIRDFNode* aTarget,
              PRBool aTruthValue);

    ~Assertion();

    // public for now, because I'm too lazy to go thru and clean this up.
    nsIRDFResource* mSource;     // OWNER
    nsIRDFResource* mProperty;   // OWNER
    nsIRDFNode*     mTarget;     // OWNER
    Assertion*      mNext;
    Assertion*      mInvNext;
    PRBool          mTruthValue;

    // For nsIRDFPurgeableDataSource
    void Mark() { mMarked = PR_TRUE; }
    PRBool IsMarked() { return mMarked; }
    void Unmark() { mMarked = PR_FALSE; }

private:
    PRBool          mMarked;
    PRInt16         mRefCnt; // XXX not used yet: to be used for threadsafety
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
    NS_ADDREF(mSource);
    NS_ADDREF(mProperty);
    NS_ADDREF(mTarget);
}

Assertion::~Assertion()
{
    NS_RELEASE(mSource);
    NS_RELEASE(mProperty);
    NS_RELEASE(mTarget);
}


    
////////////////////////////////////////////////////////////////////////
// Utility routines

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}

////////////////////////////////////////////////////////////////////////
// InMemoryDataSource
class InMemoryResourceEnumeratorImpl;

class InMemoryDataSource : public nsIRDFDataSource,
                           public nsIRDFPurgeableDataSource
{
protected:
    // These hash tables are keyed on pointers to nsIRDFResource
    // objects (the nsIRDFService ensures that there is only ever one
    // nsIRDFResource object per unique URI). The value of an entry is
    // an Assertion struct, which is a linked list of (subject
    // predicate object) triples.
    PLHashTable* mForwardArcs; 
    PLHashTable* mReverseArcs; 

    nsCOMPtr<nsISupportsArray> mObservers;  

    static const PRInt32 kInitialTableSize;

    static PRIntn DeleteForwardArcsEntry(PLHashEntry* he, PRIntn i, void* arg);

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

    nsISupports* mOuter;

public:

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource methods
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

    NS_IMETHOD Change(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aOldTarget,
                      nsIRDFNode* aNewTarget);

    NS_IMETHOD Move(nsIRDFResource* aOldSource,
                    nsIRDFResource* aNewSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aTarget);

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

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult);

    NS_IMETHOD Flush();

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                              nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

    // nsIRDFPurgeableDataSource methods
    NS_IMETHOD Mark(nsIRDFResource* aSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aTarget,
                    PRBool aTruthValue,
                    PRBool* aDidMark);

    NS_IMETHOD Sweep();

protected:
    static PRIntn SweepForwardArcsEntries(PLHashEntry* he, PRIntn i, void* arg);

public:
    // Implemenatation methods
    Assertion* GetForwardArcs(nsIRDFResource* u);
    Assertion* GetReverseArcs(nsIRDFNode* v);
    void       SetForwardArcs(nsIRDFResource* u, Assertion* as);
    void       SetReverseArcs(nsIRDFNode* v, Assertion* as);

#ifdef PR_LOGGING
    void
    LogOperation(const char* aOperation,
                 nsIRDFResource* asource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aTarget,
                 PRBool aTruthValue = PR_TRUE);
#endif

    // This datasource's monitor object.
    PRLock* mLock;
};

const PRInt32 InMemoryDataSource::kInitialTableSize = 500;

////////////////////////////////////////////////////////////////////////
// InMemoryAssertionEnumeratorImpl

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

    // XXX this implementation is a race condition waiting to
    // happen. Hopefully, no one will blow away this assertion while
    // we're iterating, but...
    Assertion*      mNextAssertion;

public:
    InMemoryAssertionEnumeratorImpl(InMemoryDataSource* aDataSource,
                                    nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue);

    virtual ~InMemoryAssertionEnumeratorImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS
   
    // nsISimpleEnumerator interface
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);
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
}

InMemoryAssertionEnumeratorImpl::~InMemoryAssertionEnumeratorImpl()
{
    NS_IF_RELEASE(mDataSource);
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mProperty);
    NS_IF_RELEASE(mTarget);
    NS_IF_RELEASE(mValue);
}

NS_IMPL_ISUPPORTS(InMemoryAssertionEnumeratorImpl, nsISimpleEnumerator::GetIID());

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

        mNextAssertion = (mSource) ? mNextAssertion->mNext : mNextAssertion->mInvNext;

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
    InMemoryDataSource* mDataSource;
    nsIRDFResource*     mSource;
    nsIRDFNode*         mTarget;
    nsVoidArray         mAlreadyReturned;
    nsIRDFResource*     mCurrent;
    Assertion*          mAssertion;

public:
    InMemoryArcsEnumeratorImpl(InMemoryDataSource* aDataSource,
                               nsIRDFResource* aSource,
                               nsIRDFNode* aTarget);

    virtual ~InMemoryArcsEnumeratorImpl();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);
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
    NS_RELEASE(mDataSource);
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mTarget);
    NS_IF_RELEASE(mCurrent);

    for (PRInt32 i = mAlreadyReturned.Count() - 1; i >= 0; --i) {
        nsIRDFResource* resource = (nsIRDFResource*) mAlreadyReturned[i];
        NS_RELEASE(resource);
    }
}

NS_IMPL_ISUPPORTS(InMemoryArcsEnumeratorImpl, nsISimpleEnumerator::GetIID());

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

    InMemoryDataSource* datasource = new InMemoryDataSource(aOuter);
    if (! datasource)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    rv = datasource->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = datasource->QueryInterface(aIID, aResult); // This'll AddRef()

        if (NS_SUCCEEDED(rv))
            return rv;
    }

    delete datasource;
    *aResult = nsnull;
    return rv;
}



InMemoryDataSource::InMemoryDataSource(nsISupports* aOuter)
    : mForwardArcs(nsnull),
      mReverseArcs(nsnull),
      mOuter(aOuter),
      mLock(nsnull)
{
    NS_INIT_REFCNT();
}


nsresult
InMemoryDataSource::Init()
{
    mForwardArcs = PL_NewHashTable(kInitialTableSize,
                                   rdf_HashPointer,
                                   PL_CompareValues,
                                   PL_CompareValues,
                                   nsnull,
                                   nsnull);

    if (! mForwardArcs)
        return NS_ERROR_OUT_OF_MEMORY;

    mReverseArcs = PL_NewHashTable(kInitialTableSize,
                                   rdf_HashPointer,
                                   PL_CompareValues,
                                   PL_CompareValues,
                                   nsnull,
                                   nsnull);

    if (! mReverseArcs)
        return NS_ERROR_OUT_OF_MEMORY;

    mLock = PR_NewLock();
    if (! mLock)
        return NS_ERROR_OUT_OF_MEMORY;

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("InMemoryDataSource");
#endif

    return NS_OK;
}


InMemoryDataSource::~InMemoryDataSource()
{
    if (mForwardArcs) {
        // This'll release all of the Assertion objects that are
        // associated with this data source. We only need to do this
        // for the forward arcs, because the reverse arcs table
        // indexes the exact same set of resources.
        PL_HashTableEnumerateEntries(mForwardArcs, DeleteForwardArcsEntry, nsnull);

        PL_HashTableDestroy(mForwardArcs);
        mForwardArcs = nsnull;
    }
    if (mReverseArcs) {
        PL_HashTableDestroy(mReverseArcs);
        mReverseArcs = nsnull;
    }

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("InMemoryDataSource(%p): destroyed.", this));

    PR_DestroyLock(mLock);
}

PRIntn
InMemoryDataSource::DeleteForwardArcsEntry(PLHashEntry* he, PRIntn i, void* arg)
{
    Assertion* as = (Assertion*) he->value;
    while (as) {
        Assertion* doomed = as;
        as = as->mNext;
        delete doomed;
    }
    return HT_ENUMERATE_NEXT;
}


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP_(nsrefcnt)
InMemoryDataSource::AddRef()
{
    if (mOuter) {
        return mOuter->AddRef();
    }
    else {
        return ++mRefCnt;
    }
}


NS_IMETHODIMP_(nsrefcnt)
InMemoryDataSource::Release()
{
    nsrefcnt refcnt;
    if (mOuter) {
        refcnt = mOuter->Release();
    }
    else {
        refcnt = --mRefCnt;
    }
    if (refcnt == 0) {
        delete this;
    }
    return refcnt;
}


NS_IMETHODIMP
InMemoryDataSource::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIRDFDataSource::GetIID()) ||
        ((!mOuter && aIID.Equals(kISupportsIID)))) {
        *aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
    }
    else if (aIID.Equals(nsIRDFPurgeableDataSource::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIRDFPurgeableDataSource*, this);
    }
    else if (mOuter) {
        return mOuter->QueryInterface(aIID, aResult);
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }

    NS_ADDREF(this);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////


Assertion*
InMemoryDataSource::GetForwardArcs(nsIRDFResource* u)
{
    // Cast is okay, we're in a closed system
    return (Assertion*) PL_HashTableLookup(mForwardArcs, u);
}

Assertion*
InMemoryDataSource::GetReverseArcs(nsIRDFNode* v)
{
    // Cast is okay, we're in a closed system
    return (Assertion*) PL_HashTableLookup(mReverseArcs, v);
}   

void
InMemoryDataSource::SetForwardArcs(nsIRDFResource* u, Assertion* as)
{
    NS_PRECONDITION(u != nsnull, "null ptr");
    if (as) {
        PL_HashTableAdd(mForwardArcs, u, as);
    }
    else {
        PL_HashTableRemove(mForwardArcs, u);
    }
}

void
InMemoryDataSource::SetReverseArcs(nsIRDFNode* v, Assertion* as)
{
    NS_PRECONDITION(v != nsnull, "null ptr");
    if (as) {
        PL_HashTableAdd(mReverseArcs, v, as);
    }
    else {
        PL_HashTableRemove(mReverseArcs, v);
    }
}


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
        char* valueCStr = valueStr.ToNewCString();

        PR_LOG(gLog, PR_LOG_ALWAYS,
           ("  -->(\"%s\")\n", valueCStr));

        delete[] valueCStr;
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

    InMemoryAssertionEnumeratorImpl* result
        = new InMemoryAssertionEnumeratorImpl(this, nsnull, aProperty, aTarget, aTruthValue);

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

    InMemoryAssertionEnumeratorImpl* result
        = new InMemoryAssertionEnumeratorImpl(this, aSource, aProperty, nsnull, aTruthValue);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}


nsresult
InMemoryDataSource::LockedAssert(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 nsIRDFNode* target,
                                 PRBool tv)
{
#ifdef PR_LOGGING
    LogOperation("ASSERT", source, property, target, tv);
#endif

    Assertion* next = GetForwardArcs(source);
    Assertion* prev = next;
    Assertion* as = nsnull;

    // Walk to the end of the linked list.
    // XXX shouldn't we just keep a pointer to the end, or insert at the front???
    while (next) {
        if (property == next->mProperty) {
            if (target == next->mTarget) {
                // Wow, we already had the assertion. Make sure that the
                // truth values are correct and bail.
                next->mTruthValue = tv;
                return NS_OK;
            }
        }

        prev = next;
        next = next->mNext;
    }

    as = new Assertion(source, property, target, tv);
    if (! as)
        return NS_ERROR_OUT_OF_MEMORY;

    // Link it in to the "forward arcs" table
    if (!prev) {
        SetForwardArcs(source, as);
    } else {
        prev->mNext = as;
    }
    
    // Link it in to the "reverse arcs" table

    // XXX Shouldn't we keep a pointer to the end of the list to make
    // sure this is O(1)?
    prev = nsnull;
    next = GetReverseArcs(target);
    while (next) {
        prev = next;
        next = next->mInvNext;
    }

    if (!prev) {
        SetReverseArcs(target, as);
    } else {
        prev->mInvNext = as;
    }

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Assert(nsIRDFResource* source,
                           nsIRDFResource* property, 
                           nsIRDFNode* target,
                           PRBool tv) 
{
    nsresult rv;

    {
        NS_AUTOLOCK(mLock);
        rv = LockedAssert(source, property, target, tv);
        if (NS_FAILED(rv)) return rv;
    }

    // notify observers
    if (mObservers) {
        PRUint32 count;
        rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnAssert(source, property, target);
            NS_RELEASE(obs);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


nsresult
InMemoryDataSource::LockedUnassert(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 nsIRDFNode* target)
{
#ifdef PR_LOGGING
    LogOperation("UNASSERT", source, property, target);
#endif

    Assertion* next = GetForwardArcs(source);
    Assertion* prev = next;
    Assertion* as = nsnull;

    while (next) {
        if (property == next->mProperty) {
            if (target == next->mTarget) {
                if (prev == next) {
                    SetForwardArcs(source, next->mNext);
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

    next = prev = GetReverseArcs(target);
    while (next) {
        if (next == as) {
            if (prev == next) {
                SetReverseArcs(target, next->mInvNext);
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

    // Delete the assertion struct & release resources
    delete as;

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Unassert(nsIRDFResource* source,
                             nsIRDFResource* property,
                             nsIRDFNode* target)
{
    nsresult rv;

    {
        NS_AUTOLOCK(mLock);

        rv = LockedUnassert(source, property, target);
        if (NS_FAILED(rv)) return rv;
    }

    // Notify the world
    if (mObservers) {
        PRUint32 count;
        rv = mObservers->Count(&count);
        if (NS_FAILED(rv)) return rv;

        for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(source, property, target);
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
            obs->OnChange(aSource, aProperty, aOldTarget, aNewTarget);
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
            obs->OnMove(aOldSource, aNewSource, aProperty, aTarget);
            NS_RELEASE(obs);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


NS_IMETHODIMP
InMemoryDataSource::AddObserver(nsIRDFObserver* observer)
{
    NS_ASSERTION(observer != nsnull, "null ptr");
    if (! observer)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    if (! mObservers) {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }

    mObservers->AppendElement(observer);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::RemoveObserver(nsIRDFObserver* observer)
{
    NS_ASSERTION(observer != nsnull, "null ptr");
    if (! observer)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    if (! mObservers)
        return NS_OK;

    mObservers->RemoveElement(observer);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::ArcLabelsIn(nsIRDFNode* aTarget, nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    InMemoryArcsEnumeratorImpl* result =
        new InMemoryArcsEnumeratorImpl(this, nsnull, aTarget);

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

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    InMemoryArcsEnumeratorImpl* result =
        new InMemoryArcsEnumeratorImpl(this, aSource, nsnull);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *aResult = result;

    return NS_OK;
}

static PRIntn
rdf_ResourceEnumerator(PLHashEntry* he, PRIntn i, void* closure)
{
    nsISupportsArray* resources = NS_STATIC_CAST(nsISupportsArray*, closure);

    nsIRDFResource* resource = 
        NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));

    NS_ADDREF(resource);
    resources->AppendElement(resource);
    return HT_ENUMERATE_NEXT;
}


NS_IMETHODIMP
InMemoryDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsISupportsArray> values;
    rv = NS_NewISupportsArray(getter_AddRefs(values));
    if (NS_FAILED(rv)) return rv;

    NS_AUTOLOCK(mLock);

    // Enumerate all of our entries into an nsISupportsArray.
    PL_HashTableEnumerateEntries(mForwardArcs, rdf_ResourceEnumerator, values.get());

    *aResult = new nsArrayEnumerator(values);
    if (! *aResult)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Flush()
{
    // XXX nothing to flush, right?
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
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InMemoryDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                              nsIRDFResource*   aCommand,
                              nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    PLHashTable* mReverseArcs;
};

NS_IMETHODIMP
InMemoryDataSource::Sweep()
{
    SweepInfo info = { nsnull, mReverseArcs };

    {
        // Remove all the assertions while holding the lock, but don't notify anyone.
        NS_AUTOLOCK(mLock);
        PL_HashTableEnumerateEntries(mForwardArcs, SweepForwardArcsEntries, &info);
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
                    obs->OnUnassert(as->mSource, as->mProperty, as->mTarget);
                    // XXX ignore return value?
                }
            }
        }

        Assertion* doomed = as;
        as = as->mNext;
        delete doomed;
    }

    return NS_OK;
}


PRIntn
InMemoryDataSource::SweepForwardArcsEntries(PLHashEntry* he, PRIntn i, void* arg)
{
    SweepInfo* info = (SweepInfo*) arg;

    Assertion* as = (Assertion*) he->value;
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
                he->value = next;
            }

            // remove from the reverse arcs
            PLHashEntry** hep =
                PL_HashTableRawLookup(info->mReverseArcs,
                                      (PLHashNumber) as->mTarget, // because we know we're using rdf_HashPointer()
                                      as->mTarget);
            
            Assertion* ras = (Assertion*) ((*hep)->value);
            NS_ASSERTION(ras != nsnull, "no assertion in reverse arcs");
            Assertion* rprev = nsnull;
            while (ras) {
                if (ras == as) {
                    if (rprev) {
                        rprev->mInvNext = ras->mInvNext;
                    }
                    else {
                        // it's the first one. update the hashtable entry.
                        (*hep)->value = ras->mInvNext;
                    }
                    as->mInvNext = nsnull; // for my sanity.
                    break;
                }
                rprev = ras;
                ras = ras->mInvNext;
            }

            // Wow, it was the _only_ one. Unhash it.
            if (! (*hep)->value)
                PL_HashTableRawRemove(info->mReverseArcs, hep, *hep);

            
            // add to the list of assertions to unassert
            as->mNext = info->mUnassertList;
            info->mUnassertList = as;

            // Advance to the next assertion
            as = next;
        }
    }

    PRIntn result = HT_ENUMERATE_NEXT;

    // if no more assertions exist for this resource, then unhash it.
    if (! he->value)
        result |= HT_ENUMERATE_REMOVE;

    return result;
}

////////////////////////////////////////////////////////////////////////

