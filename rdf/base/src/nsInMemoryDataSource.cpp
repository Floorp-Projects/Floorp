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

  TO DO:

  1) ArcsIn & ArcsOut cursors.

 */

#include "nscore.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIServiceManager.h"
#include "nsVoidArray.h"  // XXX introduces dependency on raptorbase
//Guha --- could we ask them to move it out?
#include "nsRDFCID.h"
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
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);



class Assertion 
{
public:
    nsIRDFResource* mSource;
    nsIRDFResource* mProperty;
    nsIRDFNode*     mTarget;
    PRBool          mTv;
    Assertion*      mNext;
    Assertion*      mInvNext;
}
    
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

////////////////////////////////////////////////////////////////////////
// InMemoryDataSource


class InMemoryDataSource : public nsIRDFDataSource
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

    // Implemenatation methods

    // XXX how about more descriptive names for these...
    Assertion* getArg1 (nsIRDFResource* u);
    Assertion* getArg2 (nsIRDFNode* v);
    void       setArg1 (nsIRDFResource* u, Assertion* as);
    void       setArg2 (nsIRDFNode* v, Assertion* as);
};

const PRInt32 InMemoryDataSource::kInitialTableSize = 500;

////////////////////////////////////////////////////////////////////////
// InMemoryAssertionCursor

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
    PRBool          mInversep;
    Assertion*      mNextAssertion;

public:
    InMemoryAssertionCursor(InMemoryDataSource* ds,
                            nsIRDFNode* u,
                            nsIRDFResource* s,
                            PRBool tv,
                            PRBool inversep);

    virtual ~InMemoryAssertionCursor(void);

    // nsISupports interface
    NS_DECL_ISUPPORTS
   
    // nsIRDFCursor interface
    NS_IMETHOD Advance(void);

    // nsIRDFAssertionCursor interface
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};

////////////////////////////////////////////////////////////////////////

InMemoryAssertionCursor::InMemoryAssertionCursor (InMemoryDataSource* ds,
                                                  nsIRDFNode* u,
                                                  nsIRDFResource* label, 
                                                  PRBool tv,
                                                  PRBool inversep)
    : mDataSource(ds),
      mLabel(label),
      mTruthValue(tv),
      mInversep(inversep),
      mCount(0),
      mNextAssertion(nsnull),
      mValue(nsnull)
{
    NS_INIT_REFCNT();

    mInversep = inversep;
    if (inversep) {
        mTarget = u;
        mNextAssertion = mDataSource->getArg2(u);
    } else {
        mSource = u;
        mNextAssertion = mDataSource->getArg1(mSource);
        // Dont need this ...
        //        if (NS_SUCCEEDED(u->QueryInterface(kIRDFResourceIID, (void**) &mSource)))
        //  mNextAssertion = mDataSource->getArg1(mSource);
    }
}

InMemoryAssertionCursor::~InMemoryAssertionCursor(void)
{
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

        if ((mTruthValue == mNextAssertion->mTv) && eq) {
            if (mInversep) {
                mValue = mNextAssertion->mSource;
                NS_ADDREF(mValue);
            } else {
                mValue = mNextAssertion->target;
                NS_ADDREF(mValue);
            }
            return NS_OK;
        }
        mNextAssertion = (mInversep ? mNextAssertion->mInvNext : mNextAssertion->mNext);
    }

    // If we get here, the cursor is empty.
    return NS_ERROR_RDF_CURSOR_EMPTY;
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

    if (mInversep) {
        if (! mValue)
            return NS_ERROR_UNEXPECTED;

        // this'll AddRef()
        return mValue->QueryInterface(kIRDFResourceIID, (void**) aSubject);
    }
    else {
        NS_ADDREF(mSource);
        *aSubject = mSource;
        return NS_OK;
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

    if (mInversep) {
        NS_ADDREF(mTarget);
        *aObject = mTarget;
    }
    else {
        if (! mValue)
            return NS_ERROR_UNEXPECTED;

        // this'll AddRef()
        *aObject = mValue;
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

NS_IMPL_ISUPPORTS(InMemoryDataSource, kIRDFDataSourceIID);

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
InMemoryDataSource::getArg1 (nsIRDFResource* u)
{
    // Cast is okay, we're in a closed system
    return (Assertion*) PL_HashTableLookup(mForwardArcs, u);
}

Assertion*
InMemoryDataSource::getArg2 (nsIRDFNode* v)
{
    // Cast is okay, we're in a closed system
    return (Assertion*) PL_HashTableLookup(mReverseArcs, v);
}   

void
InMemoryDataSource::setArg1 (nsIRDFResource* u, Assertion* as)
{
    PL_HashTableAdd(mForwardArcs, u, as);
}

void
InMemoryDataSource::setArg2 (nsIRDFNode* v, Assertion* as)
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
InMemoryDataSource::GetSource(nsIRDFResource* property, nsIRDFNode* target,
                              PRBool tv, nsIRDFResource** source)
{
    nsresult rv;
    for (Assertion* as = getArg2(target); as != nsnull; as = as->mNext) {
        PRBool eq;
        if (NS_FAILED(rv = property->EqualsResource(as->mProperty, &eq)))
            return rv;

        if (! eq)
            continue;

        if (as->mTv != tv)
            continue;

        *source = as->mSource;
        return NS_OK;
    }
    *source = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InMemoryDataSource::GetTarget(nsIRDFResource* source,  nsIRDFResource* property,
                              PRBool tv, nsIRDFNode** target) {
    nsresult rv;
    for (Assertion* as = getArg1(source); as != nsnull; as = as->mNext) {
        PRBool eq;
        if (NS_FAILED(rv = property->EqualsResource(as->mProperty, &eq)))
            return rv;

        if (! eq)
            continue;

        if (as->mTv != tv)
            continue;

        *target = as->target;
        return NS_OK;
    }

    // If we get here, then there was no target with for the specified
    // property & truth value.
    *target = nsnull;
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
InMemoryDataSource::HasAssertion(nsIRDFResource* source, nsIRDFResource* property,
                                 nsIRDFNode* target, PRBool tv,PRBool* hasAssertion)
{
    nsresult rv;
    for (Assertion* as = getArg1(source); as != nsnull; as = as->mNext) {
        PRBool eq;
        if (NS_FAILED(rv = property->EqualsResource(as->mProperty, &eq)))
            return rv;

        if (! eq)
            continue;

        if (NS_FAILED(rv = target->EqualsNode(as->target, &eq)))
            return rv;

        if (! eq)
            continue;

        if (as->mTv != tv)
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
InMemoryDataSource::GetSources(nsIRDFResource* property, nsIRDFNode* target,
                               PRBool tv, nsIRDFAssertionCursor** sources)
{
    *sources = new InMemoryAssertionCursor (this, target, property, tv, PR_TRUE);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::GetTargets(nsIRDFResource* source,
                               nsIRDFResource* property,
                               PRBool tv, nsIRDFAssertionCursor** targets)
{
    *targets = new InMemoryAssertionCursor (this, source, property, tv, PR_FALSE);
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSource::Assert(nsIRDFResource* source, nsIRDFResource* property, 
                           nsIRDFNode* target, PRBool tv) 
{
    nsresult rv;

    Assertion* next = getArg1(source);
    Assertion* prev = next;
    Assertion* as = nsnull;

    // Walk to the end of the linked list.
    // XXX shouldn't we just keep a pointer to the end, or insert at the front???
    while (next) {
        PRBool eq;

        if (NS_FAILED(rv = property->EqualsResource(next->mProperty, &eq)))
            return rv;

        if (eq) {
            if (NS_FAILED(rv = target->EqualsNode(next->target, &eq)))
                return rv;

            if (eq) {
                // Wow, we already had the assertion. Make sure that the
                // truth values are correct and bail.
                next->mTv = tv;
                return NS_OK;
            }
        }

        prev = next;
        next = as->mNext;
    }

    as = new Assertion;
    if (! as)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(source);
    as->mSource  = source;

    NS_ADDREF(property);
    as->mProperty  = property;

    NS_ADDREF(target);
    as->target  = target;

    as->mTv = tv;

    // Link it in to the "forward arcs" table
    if (!prev) {
        setArg1(source, as);
    } else {
        prev->mNext = as;
    }
    
    // Link it in to the "reverse arcs" table
    prev = nsnull;

    // XXX Shouldn't we keep a pointer to the end of the list to make
    // sure this is O(1)?
    for (next = getArg2(target); next != nsnull; next = next->mInvNext) {prev = next;}
    if (!prev) {
        setArg2(target, as);
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
                             nsIRDFResource* property, nsIRDFNode* target)
{
    nsresult rv;
    Assertion* next = getArg1(source);
    Assertion* prev = next;
    Assertion* as = nsnull;

    while (next) {
        PRBool eq;

        if (NS_FAILED(rv = property->EqualsResource(next->mProperty, &eq)))
            return rv;

        if (eq) {
            if (NS_FAILED(rv = target->EqualsNode(next->target, &eq)))
                return rv;

            if (eq) {
                if (prev == next) {
                    setArg1(source, next->mNext);
                } else {
                    prev->mNext = next->mNext;
                }
                as = next;
                break;
            }
        }

        prev = next;
        next = as->mNext;
    }

    // We don't even have the assertion, so just bail.
    if (!as)
        return NS_OK;

    next = prev = getArg2(target);
    while (next) {
        if (next == as) {
            if (prev == next) {
                setArg2(target, next->mInvNext);
            } else {
                prev->mInvNext = next->mInvNext;
            }
            break;
        }
        prev = next;
        next = as->mInvNext;
    }

    // XXX delete the assertion struct & release resources?

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
    // XXX implement later
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHODIMP
InMemoryDataSource::ArcLabelsOut(nsIRDFResource* source,nsIRDFArcsOutCursor** labels)
{
    // XXX implement later
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHODIMP
InMemoryDataSource::Flush()
{
    // XXX nothing to flush, right?
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
