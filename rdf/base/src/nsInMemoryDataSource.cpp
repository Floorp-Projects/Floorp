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

#include "nscore.h"
#include "nsIOutputStream.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
#include "nsString.h"
#include "rdfutil.h"
#include "plhash.h"
#include "plstr.h"


#if 1 // defined(MOZ_THREADSAFE_RDF)
#include "nsAutoLock.h"
#define NS_AUTOLOCK(__monitor) nsAutoLock __lock(__monitor)
#else
#define NS_AUTOLOCK(__monitor)
#endif


static NS_DEFINE_IID(kIRDFAssertionCursorIID,  NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsInCursorIID,     NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,    NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,           NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,          NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFNodeIID,             NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,         NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFResourceCursorIID,   NS_IRDFRESOURCECURSOR_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);

enum Direction {
    eDirectionForwards,
    eDirectionReverse
};

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
class InMemoryResourceCursor;

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

    friend class InMemoryResourceCursor; // b/c it needs to enumerate mForwardArcs

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

    NS_IMETHOD GetURI(const char* *uri) const;

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source);

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets);

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
                           nsIRDFArcsInCursor** labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels);

    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor);

    NS_IMETHOD Flush();

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments);

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
// InMemoryAssertionCursor

/**
 * InMemoryAssertionCursor
 */
class InMemoryAssertionCursor : public nsIRDFAssertionCursor
{
private:
    InMemoryDataSource* mDataSource;
    nsIRDFResource* mSource;
    nsIRDFResource* mLabel;
    nsIRDFNode*     mTarget;
    nsIRDFNode*     mValue;
    PRInt32         mCount;
    PRBool          mTruthValue;
    Direction       mDirection;

    // XXX this implementation is a race condition waiting to
    // happen. Hopefully, no one will blow away this assertion while
    // we're iterating, but...
    Assertion*      mNextAssertion;

public:
    InMemoryAssertionCursor(InMemoryDataSource* ds,
                              nsIRDFNode* u,
                              nsIRDFResource* s,
                              PRBool tv,
                              Direction mDirection);

    virtual ~InMemoryAssertionCursor(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS
   
    // nsIRDFCursor interface
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFAssertionCursor interface
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};

////////////////////////////////////////////////////////////////////////

InMemoryAssertionCursor::InMemoryAssertionCursor(InMemoryDataSource* ds,
                                                 nsIRDFNode* u,
                                                 nsIRDFResource* label, 
                                                 PRBool tv,
                                                 Direction direction)
    : mDataSource(ds),
      mSource(nsnull),
      mLabel(label),
      mTarget(nsnull),
      mTruthValue(tv),
      mDirection(direction),
      mCount(0),
      mNextAssertion(nsnull),
      mValue(nsnull)
{
    NS_INIT_REFCNT();

    NS_ADDREF(mDataSource);
    NS_ADDREF(mLabel);

    if (mDirection == eDirectionForwards) {
        // Cast is okay because we're in a closed system.
        mSource = (nsIRDFResource*) u;
        NS_ADDREF(mSource);

        mNextAssertion = mDataSource->GetForwardArcs(mSource);
    } else {
        mTarget = u;
        NS_ADDREF(mTarget);

        mNextAssertion = mDataSource->GetReverseArcs(u);
    }
}

InMemoryAssertionCursor::~InMemoryAssertionCursor(void)
{
    NS_IF_RELEASE(mDataSource);
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mLabel);
    NS_IF_RELEASE(mTarget);
    NS_IF_RELEASE(mValue);
}

NS_IMPL_ISUPPORTS(InMemoryAssertionCursor, kIRDFAssertionCursorIID);

NS_IMETHODIMP
InMemoryAssertionCursor::Advance(void)
{
    nsresult rv;

    NS_AUTOLOCK(mDataSource->mLock);

    NS_IF_RELEASE(mValue);

    while (mNextAssertion) {
        PRBool eq;
        if (NS_FAILED(rv = mLabel->EqualsResource(mNextAssertion->mProperty, &eq)))
            return rv;

        PRBool foundIt = PR_FALSE;
        if ((mTruthValue == mNextAssertion->mTruthValue) && eq) {
            if (mDirection == eDirectionForwards) {
                mValue = mNextAssertion->mTarget;
                NS_ADDREF(mValue);
            } else {
                mValue = mNextAssertion->mSource;
                NS_ADDREF(mValue);
            }
            foundIt = PR_TRUE;
        }
        mNextAssertion = (mDirection == eDirectionForwards ? 
                          mNextAssertion->mNext :
                          mNextAssertion->mInvNext);

        if (foundIt)
            return NS_OK;
    }

    // If we get here, the cursor is empty.
    return NS_ERROR_RDF_CURSOR_EMPTY;
}

NS_IMETHODIMP 
InMemoryAssertionCursor::GetValue(nsIRDFNode** aValue)
{
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mDataSource->mLock);

    NS_ADDREF(mValue);
    *aValue = mValue;
    return NS_OK;
}   

NS_IMETHODIMP
InMemoryAssertionCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mDataSource);
    *aDataSource = mDataSource;
    return NS_OK;
}


NS_IMETHODIMP
InMemoryAssertionCursor::GetSubject(nsIRDFResource** aSubject)
{
    NS_PRECONDITION(aSubject != nsnull, "null ptr");
    if (! aSubject)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mDataSource->mLock);

    if (mDirection == eDirectionForwards) {
        NS_ADDREF(mSource);
        *aSubject = mSource;
        return NS_OK;
    }
    else {
        if (! mValue)
            return NS_ERROR_UNEXPECTED;

        // this'll AddRef()
        return mValue->QueryInterface(kIRDFResourceIID, (void**) aSubject);
    }
}


NS_IMETHODIMP
InMemoryAssertionCursor::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_PRECONDITION(aPredicate != nsnull, "null ptr");
    if (! aPredicate)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mLabel);
    *aPredicate = mLabel;
    return NS_OK;
}


NS_IMETHODIMP
InMemoryAssertionCursor::GetObject(nsIRDFNode** aObject)
{
    NS_PRECONDITION(aObject != nsnull, "null ptr");
    if (! aObject)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mDataSource->mLock);

    if (mDirection == eDirectionForwards) {
        if (! mValue)
            return NS_ERROR_UNEXPECTED;

        // this'll AddRef()
        *aObject = mValue;
    }
    else {
        NS_ADDREF(mTarget);
        *aObject = mTarget;
    }
    return NS_OK;
}


NS_IMETHODIMP
InMemoryAssertionCursor::GetTruthValue(PRBool* aTruthValue)
{
    NS_PRECONDITION(aTruthValue != nsnull, "null ptr");
    if (! aTruthValue)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mDataSource->mLock);

    *aTruthValue = mTruthValue;
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

class InMemoryArcsCursor : public nsIRDFArcsOutCursor,
                           public nsIRDFArcsInCursor
{
private:
    InMemoryDataSource* mDataSource;
    nsIRDFResource*     mSubject;
    nsIRDFNode*         mObject;
    nsVoidArray         mElements;
    PRInt32             mNextIndex;
    nsIRDFResource*     mCurrent;
    Direction           mDirection;

public:
    InMemoryArcsCursor(InMemoryDataSource* ds,
                       nsIRDFNode* node,
                       Direction direction);

    virtual ~InMemoryArcsCursor(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFCursor interface
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFArcsOutCursor interface
    NS_IMETHOD GetSubject(nsIRDFResource** aSubject);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);

    // nsIRDFArcsInCursor interface
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
};

InMemoryArcsCursor::InMemoryArcsCursor(InMemoryDataSource* ds,
                                       nsIRDFNode* node,
                                       Direction direction)
    : mDataSource(ds),
      mSubject(nsnull),
      mObject(nsnull),
      mNextIndex(0),
      mCurrent(nsnull),
      mDirection(direction)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDataSource);

    // Hopefully this won't suck too much because most arcs will have
    // a small number of properties; or at worst, a small number of
    // unique properties in the case of multi-attributes. This breaks
    // for RDF container elements, which we should eventually special
    // case.

    Assertion* as;
    if (mDirection == eDirectionForwards) {
        // cast okay because it's a closed system
        mSubject = (nsIRDFResource*) node;
        NS_ADDREF(mSubject);
        as = ds->GetForwardArcs(mSubject);
    }
    else {
        mObject = node;
        NS_ADDREF(mObject);
        as = ds->GetReverseArcs(mObject);
    }

    while (as != nsnull) {
        PRBool alreadyHadIt = PR_FALSE;

        for (PRInt32 i = mElements.Count() - 1; i >= 0; --i) {
            if (mElements[i] == as->mProperty) {
                alreadyHadIt = PR_TRUE;
                break;
            }
        }

        if (! alreadyHadIt) {
            mElements.AppendElement(as->mProperty);
            NS_ADDREF(as->mProperty);
        }

        as = ((mDirection == eDirectionForwards) ? as->mNext : as->mInvNext);
    }
}

InMemoryArcsCursor::~InMemoryArcsCursor(void)
{
    NS_RELEASE(mDataSource);
    NS_IF_RELEASE(mSubject);
    NS_IF_RELEASE(mObject);
    NS_IF_RELEASE(mCurrent);

    for (PRInt32 i = mElements.Count() - 1; i >= mNextIndex; --i) {
        nsIRDFResource* resource = (nsIRDFResource*) mElements[i];
        NS_RELEASE(resource);
    }
}

NS_IMPL_ADDREF(InMemoryArcsCursor);
NS_IMPL_RELEASE(InMemoryArcsCursor);

NS_IMETHODIMP
InMemoryArcsCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kISupportsIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kIRDFArcsOutCursorIID)) {
        *result = NS_STATIC_CAST(nsIRDFArcsOutCursor*, this);
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(kIRDFArcsInCursorIID)) {
        *result = NS_STATIC_CAST(nsIRDFArcsInCursor*, this);
        AddRef();
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}

NS_IMETHODIMP
InMemoryArcsCursor::Advance(void)
{
    NS_IF_RELEASE(mCurrent);

    if (mNextIndex >= mElements.Count())
        return NS_ERROR_RDF_CURSOR_EMPTY;

    // Cast is ok because this is a closed system. This code
    // effectively "transfers" the reference from the array to
    // mCurrent, keeping the refcount properly in sync. We just need
    // to remember that any of the indicies before mNextIndex are
    // dangling pointers.
    mCurrent = (nsIRDFResource*) mElements[mNextIndex];
    ++mNextIndex;

    return NS_OK;
}

NS_IMETHODIMP
InMemoryArcsCursor::GetValue(nsIRDFNode** aValue)
{
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mCurrent);
    *aValue = mCurrent;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryArcsCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_PRECONDITION(aDataSource != nsnull, "null ptr");
    if (! aDataSource)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mDataSource);
    *aDataSource = mDataSource;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryArcsCursor::GetSubject(nsIRDFResource** aSubject)
{
    NS_PRECONDITION(aSubject != nsnull, "null ptr");
    if (! aSubject)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mSubject);
    *aSubject = mSubject;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryArcsCursor::GetObject(nsIRDFNode** aObject)
{
    NS_PRECONDITION(aObject != nsnull, "null ptr");
    if (! aObject)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(mObject);
    *aObject = mObject;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryArcsCursor::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_PRECONDITION(aPredicate != nsnull, "null ptr");
    if (! aPredicate)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(mCurrent != nsnull, "cursor overrun");
    if (! mCurrent)
        return NS_ERROR_UNEXPECTED;

    NS_ADDREF(mCurrent);
    *aPredicate = mCurrent;
    return NS_OK;
}


NS_IMETHODIMP
InMemoryArcsCursor::GetTruthValue(PRBool* aTruthValue)
{
    *aTruthValue = PR_TRUE; // XXX need to worry about this some day...
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// InMemoryResourceCursor

class InMemoryResourceCursor : public nsIRDFResourceCursor
{
private:
    nsIRDFDataSource* mDataSource;
    nsVoidArray mResources;
    PRInt32 mNext;

public:
    InMemoryResourceCursor(InMemoryDataSource* ds);
    virtual ~InMemoryResourceCursor(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFCursor interface
    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFResourceCursor interface
    NS_IMETHOD GetResource(nsIRDFResource** aResource);
};

static PRIntn
rdf_ResourceEnumerator(PLHashEntry* he, PRIntn index, void* closure)
{
    nsVoidArray* resources = NS_STATIC_CAST(nsVoidArray*, closure);

    nsIRDFResource* resource = 
        NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));

    NS_ADDREF(resource);
    resources->AppendElement(resource);
    return HT_ENUMERATE_NEXT;
}

InMemoryResourceCursor::InMemoryResourceCursor(InMemoryDataSource* ds)
    : mDataSource(ds),
      mNext(-1)
{
    NS_INIT_REFCNT();
    mDataSource = ds;
    NS_ADDREF(ds);

    // XXX Ick. To fix this, we need to write our own hash table
    // implementation. Takers?
    PL_HashTableEnumerateEntries(ds->mForwardArcs, rdf_ResourceEnumerator, &mResources);
}

InMemoryResourceCursor::~InMemoryResourceCursor(void)
{
    for (PRInt32 i = mResources.Count() - 1; i >= mNext; --i) {
        nsIRDFResource* resource = NS_STATIC_CAST(nsIRDFResource*, mResources[i]);
        NS_RELEASE(resource);
    }
    NS_RELEASE(mDataSource);
}


NS_IMPL_ISUPPORTS(InMemoryResourceCursor, kIRDFResourceCursorIID);

NS_IMETHODIMP
InMemoryResourceCursor::Advance(void)
{
    if (mNext >= mResources.Count())
        return NS_ERROR_RDF_CURSOR_EMPTY;

    if (mNext >= 0) {
        nsIRDFResource* resource = NS_STATIC_CAST(nsIRDFResource*, mResources[mNext]);
        NS_RELEASE(resource);
    }

    ++mNext;

    if (mNext >= mResources.Count())
        return NS_ERROR_RDF_CURSOR_EMPTY;

    return NS_OK;
}

NS_IMETHODIMP
InMemoryResourceCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_ADDREF(mDataSource);
    *aDataSource = mDataSource;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryResourceCursor::GetValue(nsIRDFNode** aValue)
{
    nsresult rv;
    nsIRDFResource* result;
    if (NS_FAILED(rv = GetResource(&result)))
        return rv;

    *aValue = result; // already addref-ed
    return NS_OK;
}

NS_IMETHODIMP
InMemoryResourceCursor::GetResource(nsIRDFResource** aResource)
{
    NS_ASSERTION(mNext >= 0, "didn't advance");
    if (mNext < 0)
        return NS_ERROR_UNEXPECTED;

    NS_ASSERTION(mNext < mResources.Count(), "past end of cursor");
    if (mNext > mResources.Count())
        return NS_ERROR_UNEXPECTED;

    nsIRDFResource* result = NS_STATIC_CAST(nsIRDFResource*, mResources[mNext]);
    *aResource = result;
    NS_ADDREF(result);
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
        iid.Equals(kIRDFDataSourceIID)) {
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

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetURI(const char* *uri) const
{
    NS_PRECONDITION(uri != nsnull, "null ptr");
    if (! uri)
        return NS_ERROR_NULL_POINTER;

    *uri = mURL;
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
    return NS_ERROR_RDF_NO_VALUE;
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
    return NS_ERROR_RDF_NO_VALUE;
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
InMemoryDataSource::GetSources(nsIRDFResource* property,
                               nsIRDFNode* target,
                               PRBool tv,
                               nsIRDFAssertionCursor** sources)
{
    NS_PRECONDITION(sources != nsnull, "null ptr");
    if (! sources)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    InMemoryAssertionCursor* result
        = new InMemoryAssertionCursor(this, target, property, tv, eDirectionReverse);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *sources = result;
    NS_ADDREF(result);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetTargets(nsIRDFResource* source,
                               nsIRDFResource* property,
                               PRBool tv,
                               nsIRDFAssertionCursor** targets)
{
    NS_PRECONDITION(targets != nsnull, "null ptr");
    if (! targets)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    InMemoryAssertionCursor* result
        = new InMemoryAssertionCursor(this, source, property, tv, eDirectionForwards);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *targets = result;
    NS_ADDREF(result);
    return NS_OK;
}


nsresult
InMemoryDataSource::SafeAssert(nsIRDFResource* source,
                               nsIRDFResource* property,
                               nsIRDFNode* target,
                               PRBool tv)
{
    NS_AUTOLOCK(mLock);

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

    return NS_OK;
}


nsresult
InMemoryDataSource::SafeUnassert(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 nsIRDFNode* target)
{
    NS_AUTOLOCK(mLock);

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

    return NS_OK;
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
InMemoryDataSource::ArcLabelsIn(nsIRDFNode* node, nsIRDFArcsInCursor** labels)
{
    NS_PRECONDITION(labels != nsnull, "null ptr");
    if (! labels)
        return NS_ERROR_NULL_POINTER;

    InMemoryArcsCursor* result =
        new InMemoryArcsCursor(this, node, eDirectionReverse);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *labels = result;
    NS_ADDREF(result);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::ArcLabelsOut(nsIRDFResource* source, nsIRDFArcsOutCursor** labels)
{
    NS_PRECONDITION(labels != nsnull, "null ptr");
    if (! labels)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    InMemoryArcsCursor* result =
        new InMemoryArcsCursor(this, source, eDirectionForwards);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *labels = result;
    NS_ADDREF(result);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetAllResources(nsIRDFResourceCursor** aCursor)
{
    NS_PRECONDITION(aCursor != nsnull, "null ptr");
    if (! aCursor)
        return NS_ERROR_NULL_POINTER;

    NS_AUTOLOCK(mLock);

    InMemoryResourceCursor* result = 
        new InMemoryResourceCursor(this);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *aCursor = result;
    NS_ADDREF(result);
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
                                     nsISupportsArray/*<nsIRDFResource>*/* aArguments)
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
