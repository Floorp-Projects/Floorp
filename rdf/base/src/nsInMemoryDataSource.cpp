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
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsAutoLock.h"
#include "nsEnumeratorUtils.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

// This struct is used as the slot value in the forward and reverse
// arcs hash tables.
struct Assertion 
{
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    PRBool          mTruthValue;
    Assertion*      mNext;
    Assertion*      mInvNext;
};
    
////////////////////////////////////////////////////////////////////////
// Utility routines

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}

static PRIntn
rdf_CompareNodes(const void* v1, const void* v2)
{
    nsIRDFNode* a = (nsIRDFNode*)v1;
    nsIRDFNode* b = (nsIRDFNode*)v2;

    PRBool result;
    if (NS_FAILED(a->EqualsNode(b, &result)))
        return 0;

    return (PRIntn) result;
}


////////////////////////////////////////////////////////////////////////
// InMemoryDataSource
class InMemoryResourceEnumeratorImpl;

class InMemoryDataSource : public nsIRDFDataSource
{
protected:
    char*        mURL;

    // These hash tables are keyed on pointers to nsIRDFResource
    // objects (the nsIRDFService ensures that there is only ever one
    // nsIRDFResource object per unique URI). The value of an entry is
    // an Assertion struct, which is a linked list of (subject
    // predicate object) triples.
    PLHashTable* mForwardArcs; 
    PLHashTable* mReverseArcs; 

    nsVoidArray* mObservers;  

    static const PRInt32 kInitialTableSize;

    static PRIntn DeleteForwardArcsEntry(PLHashEntry* he, PRIntn index, void* arg);

    friend class InMemoryResourceEnumeratorImpl; // b/c it needs to enumerate mForwardArcs

    // Thread-safe writer implementation methods.
    nsresult
    SafeAssert(nsIRDFResource* source, 
               nsIRDFResource* property, 
               nsIRDFNode* target,
               PRBool tv);

    nsresult
    SafeUnassert(nsIRDFResource* source,
                 nsIRDFResource* property,
                 nsIRDFNode* target);


public:
    InMemoryDataSource(void);
    virtual ~InMemoryDataSource(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource methods
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

    NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult);

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

    // Implemenatation methods
    Assertion* GetForwardArcs(nsIRDFResource* u);
    Assertion* GetReverseArcs(nsIRDFNode* v);
    void       SetForwardArcs(nsIRDFResource* u, Assertion* as);
    void       SetReverseArcs(nsIRDFNode* v, Assertion* as);


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

    virtual ~InMemoryAssertionEnumeratorImpl(void);

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
      mTruthValue(aTruthValue),
      mCount(0),
      mNextAssertion(nsnull),
      mValue(nsnull)
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

InMemoryAssertionEnumeratorImpl::~InMemoryAssertionEnumeratorImpl(void)
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

InMemoryArcsEnumeratorImpl::~InMemoryArcsEnumeratorImpl(void)
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

NS_IMPL_ADDREF(InMemoryDataSource);
NS_IMPL_RELEASE(InMemoryDataSource);

NS_IMETHODIMP
InMemoryDataSource::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kISupportsIID) ||
        iid.Equals(nsIRDFDataSource::GetIID())) {
        *result = NS_STATIC_CAST(nsIRDFDataSource*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}

InMemoryDataSource::InMemoryDataSource(void)
    : mURL(nsnull),
      mForwardArcs(nsnull),
      mReverseArcs(nsnull),
      mObservers(nsnull),
      mLock(nsnull)
{
    mForwardArcs = PL_NewHashTable(kInitialTableSize,
                                   rdf_HashPointer,
                                   PL_CompareValues,
                                   PL_CompareValues,
                                   nsnull,
                                   nsnull);

    mReverseArcs = PL_NewHashTable(kInitialTableSize,
                                   rdf_HashPointer,
                                   rdf_CompareNodes,
                                   PL_CompareValues,
                                   nsnull,
                                   nsnull);

    mLock = PR_NewLock();

    NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("InMemoryDataSource");
#endif
}

InMemoryDataSource::~InMemoryDataSource(void)
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
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            NS_RELEASE(obs);
        }
        delete mObservers;
    }

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("InMemoryDataSource(%s): destroyed.", mURL));

    if (mURL) PL_strfree(mURL);
    PR_DestroyLock(mLock);
}

PRIntn
InMemoryDataSource::DeleteForwardArcsEntry(PLHashEntry* he, PRIntn index, void* arg)
{
    Assertion* as = (Assertion*) he->value;
    while (as) {
        Assertion* doomed = as;
        as = as->mNext;

        NS_RELEASE(doomed->mSource);
        NS_RELEASE(doomed->mProperty);
        NS_RELEASE(doomed->mTarget);
        delete doomed;
    }
    return HT_ENUMERATE_NEXT;
}


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

NS_IMETHODIMP
InMemoryDataSource::Init(const char* uri)
{
    if ((mURL = PL_strdup(uri)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("InMemoryDataSource(%s): initialized.", mURL));

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetURI(char* *uri)
{
    NS_PRECONDITION(uri != nsnull, "null ptr");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    if ((*uri = nsXPIDLCString::Copy(mURL)) == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    else
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

    nsresult rv;
    for (Assertion* as = GetReverseArcs(target); as != nsnull; as = as->mNext) {
        PRBool eq;
        if (NS_FAILED(rv = property->EqualsResource(as->mProperty, &eq)))
            return rv;

        if (! eq)
            continue;

        if (as->mTruthValue != tv)
            continue;

        *source = as->mSource;
        NS_ADDREF(as->mSource);
        return NS_OK;
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

    nsresult rv;
    for (Assertion* as = GetForwardArcs(source); as != nsnull; as = as->mNext) {
        PRBool eq;
        if (NS_FAILED(rv = property->EqualsResource(as->mProperty, &eq)))
            return rv;

        if (! eq)
            continue;

        if (as->mTruthValue != tv)
            continue;

        *target = as->mTarget;
        NS_ADDREF(as->mTarget);
        return NS_OK;
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

    nsresult rv;
    for (Assertion* as = GetForwardArcs(source); as != nsnull; as = as->mNext) {
        PRBool eq;
        if (NS_FAILED(rv = property->EqualsResource(as->mProperty, &eq)))
            return rv;

        if (! eq)
            continue;

        if (NS_FAILED(rv = target->EqualsNode(as->mTarget, &eq)))
            return rv;

        if (! eq)
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
InMemoryDataSource::SafeAssert(nsIRDFResource* source,
                               nsIRDFResource* property,
                               nsIRDFNode* target,
                               PRBool tv)
{
    NS_AUTOLOCK(mLock);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
        nsXPIDLCString uri;
        source->GetValue(getter_Copies(uri));
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("InMemoryDataSource(%s):", mURL));

        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("ASSERT  [(%p)%s]--", source, (const char*) uri));

        property->GetValue(getter_Copies(uri));
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("        --%c[(%p)%s]--", (tv ? '-' : '!'), property, (const char*) uri));

        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        if ((resource = do_QueryInterface(target)) != nsnull) {
            resource->GetValue(getter_Copies(uri));
            PR_LOG(gLog, PR_LOG_ALWAYS,
               ("        -->[(%p)%s]", target, (const char*) uri));
        }
        else if ((literal = do_QueryInterface(target)) != nsnull) {
            nsXPIDLString value;
            literal->GetValue(getter_Copies(value));
            nsAutoString valueStr(value);
            char* valueCStr = valueStr.ToNewCString();

            PR_LOG(gLog, PR_LOG_ALWAYS,
               ("        -->(\"%s\")\n", valueCStr));

            delete[] valueCStr;
        }
        else {
            PR_LOG(gLog, PR_LOG_ALWAYS,
               ("        -->(unknown-type)\n"));
        }
    }
#endif

    nsresult rv;
    Assertion* next = GetForwardArcs(source);
    Assertion* prev = next;
    Assertion* as = nsnull;

    // Walk to the end of the linked list.
    // XXX shouldn't we just keep a pointer to the end, or insert at the front???
    while (next) {
        PRBool eq;

        if (NS_FAILED(rv = property->EqualsResource(next->mProperty, &eq)))
            return rv;

        if (eq) {
            if (NS_FAILED(rv = target->EqualsNode(next->mTarget, &eq)))
                return rv;

            if (eq) {
                // Wow, we already had the assertion. Make sure that the
                // truth values are correct and bail.
                next->mTruthValue = tv;
                return NS_OK;
            }
        }

        prev = next;
        next = next->mNext;
    }

    as = new Assertion;
    if (! as)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(source);
    as->mSource = source;

    NS_ADDREF(property);
    as->mProperty = property;

    NS_ADDREF(target);
    as->mTarget = target;

    as->mTruthValue = tv;
    as->mNext       = nsnull;
    as->mInvNext    = nsnull;

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

    if (NS_FAILED(rv = SafeAssert(source, property, target, tv)))
        return rv;

    // notify observers
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnAssert(source, property, target);
            // XXX ignore return value?
        }
    }

    return NS_RDF_ASSERTION_ACCEPTED;
}


nsresult
InMemoryDataSource::SafeUnassert(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 nsIRDFNode* target)
{
    NS_AUTOLOCK(mLock);

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gLog, PR_LOG_ALWAYS)) {
        nsXPIDLCString uri;
        source->GetValue(getter_Copies(uri));
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("InMemoryDataSource(%s):", mURL));

        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("UNASSERT [(%p)%s]--", source, (const char*) uri));

        property->GetValue(getter_Copies(uri));
        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("         ---[(%p)%s]--", property, (const char*) uri));

        nsCOMPtr<nsIRDFResource> resource;
        nsCOMPtr<nsIRDFLiteral> literal;

        if ((resource = do_QueryInterface(target)) != nsnull) {
            resource->GetValue(getter_Copies(uri));
            PR_LOG(gLog, PR_LOG_ALWAYS,
               ("         -->[(%p)%s]", target, (const char*) uri));
        }
        else if ((literal = do_QueryInterface(target)) != nsnull) {
            nsXPIDLString value;
            literal->GetValue(getter_Copies(value));
            nsAutoString valueStr(value);
            char* valueCStr = valueStr.ToNewCString();

            PR_LOG(gLog, PR_LOG_ALWAYS,
               ("         -->(\"%s\")\n", valueCStr));

            delete[] valueCStr;
        }
        else {
            PR_LOG(gLog, PR_LOG_ALWAYS,
               ("         -->(unknown-type)\n"));
        }
    }
#endif

    nsresult rv;
    Assertion* next = GetForwardArcs(source);
    Assertion* prev = next;
    Assertion* as = nsnull;

    while (next) {
        PRBool eq;

        if (NS_FAILED(rv = property->EqualsResource(next->mProperty, &eq)))
            return rv;

        if (eq) {
            if (NS_FAILED(rv = target->EqualsNode(next->mTarget, &eq)))
                return rv;

            if (eq) {
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
    NS_RELEASE(as->mSource);
    NS_RELEASE(as->mProperty);
    NS_RELEASE(as->mTarget);
    delete as;

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Unassert(nsIRDFResource* source,
                             nsIRDFResource* property,
                             nsIRDFNode* target)
{
    nsresult rv;

    if (NS_FAILED(rv = SafeUnassert(source, property, target)))
        return rv;

    // Notify the world
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(source, property, target);
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
        if ((mObservers = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
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
rdf_ResourceEnumerator(PLHashEntry* he, PRIntn index, void* closure)
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

nsresult NS_NewRDFInMemoryDataSource(nsIRDFDataSource** result);
nsresult
NS_NewRDFInMemoryDataSource(nsIRDFDataSource** result)
{
    InMemoryDataSource* ds = new InMemoryDataSource();
    if (! ds)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}
