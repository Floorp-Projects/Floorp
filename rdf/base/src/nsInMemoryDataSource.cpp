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

 */

#include "nscore.h"
#include "nsIOutputStream.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFXMLSource.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
#include "nsRDFCID.h"
#include "nsString.h"
#include "rdfutil.h"
#include "plhash.h"
#include "plstr.h"

static NS_DEFINE_IID(kIRDFAssertionCursorIID,  NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsInCursorIID,     NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,    NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,           NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,       NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,          NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFNodeIID,             NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,         NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFXMLSourceIID,        NS_IRDFXMLSOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);

enum Direction {
    eDirectionForwards,
    eDirectionReverse
};

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

static nsresult
rdf_BlockingWrite(nsIOutputStream* stream, const char* buf, PRUint32 size)
{
    PRUint32 written = 0;
    PRUint32 remaining = size;
    while (remaining > 0) {
        nsresult rv;
        PRUint32 cb;

        if (NS_FAILED(rv = stream->Write(buf, written, remaining, &cb)))
            return rv;

        written += cb;
        remaining -= cb;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// InMemoryDataSource


class InMemoryDataSource : public nsIRDFDataSource,
                           public nsIRDFXMLSource
{
protected:
    char*        mURL;


    PLHashTable* mForwardArcs; 
    PLHashTable* mReverseArcs; 

    nsVoidArray* mObservers;  

    static const PRInt32 kInitialTableSize;

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

    NS_IMETHOD Flush();

    // nsIRDFXMLSource methods
    NS_IMETHOD Serialize(nsIOutputStream* aStream);

    // Implemenatation methods
    Assertion* GetForwardArcs(nsIRDFResource* u);
    Assertion* GetReverseArcs(nsIRDFNode* v);
    void       SetForwardArcs(nsIRDFResource* u, Assertion* as);
    void       SetReverseArcs(nsIRDFNode* v, Assertion* as);
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
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFAssertionCursor interface
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
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
    // XXX I don't think that the semantics of this are quite right:
    // specifically, I think that the initial Advance() will skip the
    // first element...
    // Guha --- I am pretty sure it won't
    nsresult rv;

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
 * out for is the mutliple inheiritance.
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
    NS_IMETHOD GetValue(nsIRDFNode** aValue);

    // nsIRDFArcsOutCursor interface
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
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
    NS_ADDREF(mDataSource);

    // Hopefully this won't suck too much, because most arcs will have
    // a small number of properties; or at worst, a small number of
    // unique properties in the case of multiattributes. This breaks
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
                alreadyHadIt = PR_FALSE;
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
    else if (iid.Equals(kIRDFXMLSourceIID)) {
        *result = NS_STATIC_CAST(nsIRDFXMLSource*, this);
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
      mObservers(nsnull)
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
     NS_INIT_REFCNT();
}

InMemoryDataSource::~InMemoryDataSource(void)
{
    if (mForwardArcs) {
        PL_HashTableDestroy(mForwardArcs);
        mForwardArcs = nsnull;
    }
    if (mReverseArcs) {
        PL_HashTableDestroy(mReverseArcs);
        mReverseArcs = nsnull;
    }
    if (mObservers) {
        for (PRInt32 i = mObservers->Count(); i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            NS_RELEASE(obs);
        }
        delete mObservers;
    }
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
    PL_HashTableAdd(mForwardArcs, u, as);
}

void
InMemoryDataSource::SetReverseArcs(nsIRDFNode* v, Assertion* as)
{
    PL_HashTableAdd(mReverseArcs, v, as);
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

    InMemoryAssertionCursor* result
        = new InMemoryAssertionCursor(this, source, property, tv, eDirectionForwards);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *targets = result;
    NS_ADDREF(result);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Assert(nsIRDFResource* source,
                             nsIRDFResource* property, 
                             nsIRDFNode* target,
                             PRBool tv) 
{
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
    prev = nsnull;

    // XXX Shouldn't we keep a pointer to the end of the list to make
    // sure this is O(1)?
    for (next = GetReverseArcs(target); next != nsnull; next = next->mInvNext) {prev = next;}
    if (!prev) {
        SetReverseArcs(target, as);
    } else {
        prev->mInvNext = as;
    }

    // notify observers
    if (mObservers) {
        for (PRInt32 i = mObservers->Count(); i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnAssert(source, property, target);
            // XXX ignore return value?
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Unassert(nsIRDFResource* source,
                               nsIRDFResource* property,
                               nsIRDFNode* target)
{
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

    next = prev = GetReverseArcs(target);
    while (next) {
        if (next == as) {
            if (prev == next) {
                SetReverseArcs(target, next->mInvNext);
            } else {
                prev->mInvNext = next->mInvNext;
            }
            break;
        }
        prev = next;
        next = next->mInvNext;
    }

    // Delete the assertion struct & release resources
    NS_RELEASE(as->mSource);
    NS_RELEASE(as->mProperty);
    NS_RELEASE(as->mTarget);
    delete as;

    // Notify the world
    if (mObservers) {
        for (PRInt32 i = mObservers->Count(); i >= 0; --i) {
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

    InMemoryArcsCursor* result =
        new InMemoryArcsCursor(this, source, eDirectionForwards);

    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *labels = result;
    NS_ADDREF(result);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Flush()
{
    // XXX nothing to flush, right?
    return NS_OK;
}

static void
rdf_MakeQName(nsIRDFResource* resource, char* buf, PRInt32 size)
{
    const char* uri;
    resource->GetValue(&uri);

    char* tag;

    tag = PL_strrchr(uri, '#');
    if (tag)
        goto done;

    tag = PL_strrchr(uri, '/');
    if (tag)
        goto done;

    tag = NS_CONST_CAST(char*, uri);

done:
    PL_strncpyz(buf, tag, size);
}

static PRIntn
rdf_SerializeEnumerator(PLHashEntry* he, PRIntn index, void* closure)
{
    nsIOutputStream* stream = NS_STATIC_CAST(nsIOutputStream*, closure);

    nsIRDFResource* node = 
        NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));

    const char* s;
    node->GetValue(&s);

    static const char* kRDFDescription1 = "  <RDF:Description about=\"";
    static const char* kRDFDescription2 = "\">\n";
    
    rdf_BlockingWrite(stream, kRDFDescription1, PL_strlen(kRDFDescription1));
    rdf_BlockingWrite(stream, s, PL_strlen(s));
    rdf_BlockingWrite(stream, kRDFDescription2, PL_strlen(kRDFDescription2));

    for (Assertion* as = (Assertion*) he->value; as != nsnull; as = as->mNext) {
        char qname[64];
        rdf_MakeQName(as->mProperty, qname, sizeof qname);

        rdf_BlockingWrite(stream, "    <", 5);
        rdf_BlockingWrite(stream, qname, PL_strlen(qname));
        rdf_BlockingWrite(stream, ">", 1);

        nsIRDFResource* resource;
        nsIRDFLiteral* literal;

        if (NS_SUCCEEDED(as->mTarget->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
            const char* uri;
            resource->GetValue(&uri);
            rdf_BlockingWrite(stream, uri, PL_strlen(uri));
            NS_RELEASE(resource);
        }
        else if (NS_SUCCEEDED(as->mTarget->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
            const PRUnichar* value;
            literal->GetValue(&value);
            nsAutoString s(value);

            char buf[256];
            char* p = buf;

            if (s.Length() >= sizeof(buf))
                p = new char[s.Length() + 1];

            rdf_BlockingWrite(stream, s.ToCString(p, s.Length() + 1), s.Length());

            if (p != buf)
                delete[](p);

            NS_RELEASE(literal);
        }
        else {
            PR_ASSERT(0);
        }

        rdf_BlockingWrite(stream, "    </", 6);
        rdf_BlockingWrite(stream, qname, PL_strlen(qname));
        rdf_BlockingWrite(stream, ">\n", 2);
    }

    static const char* kRDFDescription3 = "  </RDF:Description>\n";
    rdf_BlockingWrite(stream, kRDFDescription3, PL_strlen(kRDFDescription3));

    return HT_ENUMERATE_NEXT;
}

NS_IMETHODIMP
InMemoryDataSource::Serialize(nsIOutputStream* aStream)
{
    static const char* kOpenRDF  = "<RDF:RDF xmlns:RDF=\"http://www.w3.org/TR/WD-rdf-syntax#\">\n";
    static const char* kCloseRDF = "</RDF:RDF>\n";

    nsresult rv;
    if (NS_FAILED(rv = rdf_BlockingWrite(aStream, kOpenRDF, PL_strlen(kOpenRDF))))
        return rv;

    PL_HashTableEnumerateEntries(mForwardArcs, rdf_SerializeEnumerator, aStream);

    if (NS_FAILED(rv = rdf_BlockingWrite(aStream, kCloseRDF, PL_strlen(kCloseRDF))))
        return rv;

    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

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
