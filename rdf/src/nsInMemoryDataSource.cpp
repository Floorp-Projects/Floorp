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

  1) Maybe convert struct Assertion to an object?

 */

#include "nscore.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"
#include "nsString.h"
#include "rdfutil.h"
#include "plhash.h"

static NS_DEFINE_IID(kIRDFCursorIID,     NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,    NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFNodeIID,       NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,   NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

#define DEFAULT_OBSERVER_ARRAY_LENGTH 10

typedef struct _AssertionStruct {
    nsIRDFResource* u;
    nsIRDFResource* s;
    nsIRDFNode*     v;
    PRBool          tv;
    struct _AssertionStruct* next;
    struct _AssertionStruct* invNext;
} AssertionStruct;

typedef AssertionStruct* Assertion;

////////////////////////////////////////////////////////////////////////

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}

static int
rdf_CompareLiterals (const void* v1, const void* v2) {
    nsIRDFNode* a = (nsIRDFNode*)v1;
    nsIRDFNode* b = (nsIRDFNode*)v1;
    return (a->Equals(b));
}

////////////////////////////////////////////////////////////////////////



class InMemoryDataSource : public nsIRDFDataSource {
protected:
    char*        mURL;
    PLHashTable* mArg1;
    PLHashTable* mArg2;
    nsIRDFObserver** mObservers;  
    size_t       mObserverArrayLength;
    size_t       mObserverCount; // should probably use vectors here
    Assertion getArg1 (nsIRDFResource* u);
    Assertion getArg2 (nsIRDFNode* v);
    nsresult  setArg1 (nsIRDFResource* u, Assertion as);
    nsresult  setArg2 (nsIRDFNode* v, Assertion as);

public:
    InMemoryDataSource(const char* url);
    virtual ~InMemoryDataSourceImpl(void);

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
                          nsIRDFCursor** sources);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFCursor** targets);

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
                           nsIRDFCursor** labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFCursor** labels);

    NS_IMETHOD Flush();
};

NS_IMPL_ISUPPORTS(InMemoryDataSource, kIRDFDataSourceIID);

InMemoryDataSource::InMemoryDataSource (const char* url) {
    mURL = copyString(url);
    mArg1 = PL_NewHashTable(500, rdf_HashPointer, PL_CompareValues,
                            PL_CompareValues,  NULL, NULL);
    mArg2 = PL_NewHashTable(500, rdf_HashPointer, rdf_CompareLiterals,
                            PL_CompareValues,  NULL, NULL);
    mObservers = (nsIRDFObserver**)getMem(DEFAULT_OBSERVER_ARRAY_LENGTH *
                                          sizeof(nsIRDFObserver*));
    mObserverArrayLength = DEFAULT_OBSERVER_ARRAY_LENGTH;
    mObserverCount = 0;
    
}

InMemoryDataSource::~InMemoryDataSource(void)
{
    PL_HashTableDestroy(mArg1);
    PL_HashTableDestroy(mArg2);
}

Assertion 
InMemoryDataSource::getArg1 (nsIRDFResource* u) {
    return PL_HashTableLookup(mArg1, u);
}

Assertion 
InMemoryDataSource::getArg2 (nsIRDFNode* v) {
    return PL_HashTableLookup(mArg2, u);
}   

nsresult  
InMemoryDataSource::setArg1 (nsIRDFResource* u, Assertion as) {
    PL_HashTableAdd(mArg1, u, as);
    return NS_OK;
}

nsresult  
InMemoryDataSource::setArg2 (nsIRDFNode* v, Assertion as) {
    PL_HashTableAdd(mArg2, u, as);
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::GetSource(nsIRDFResource* property, nsIRDFNode* target,
                              PRBool tv, nsIRDFResource** source) {
    Assertion as = getArg2(target);
    while (as) {
        if ((as->s == property) && (as->tv == tv)) {
            **source = as->u;
            return NS_OK;
        }
        as = as->invNext;
    }
    **source = null;
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::GetTarget(nsIRDFResource* source,  nsIRDFResource* property,
                              PRBool tv, nsIRDFNode** target) {
    Assertion as = getArg1(source);
    while (as) {
        if ((as->s == property) && (as->tv == tv)) {
            **target = as->u;
            return NS_OK;
        }
        as = as->next;
    }
    **target = null;
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::HasAssertion(nsIRDFResource* source, nsIRDFResource* property,
                                       nsIRDFNode* target, PRBool tv,PRBool* hasAssertion) {
    Assertion as = getArg1(source);
    while (as) {
        if ((as->s == property) && (as->tv == tv) && (as->v == target)) {
            *hasAssertion = 1;
            return NS_OK;
        }
        as = as->next;
    }
    *hasAssertion = 0;
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::GetSources(nsIRDFResource* property,nsIRDFNode* target,
           PRBool tv, nsIRDFCursor** sources) {
    **sources = new InMemoryDataSourceCursor (target, property, tv, 1);
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::GetTargets(nsIRDFResource* source,
           nsIRDFResource* property,
           PRBool tv, nsIRDFCursor** targets) {
    **targets = new InMemoryDataSourceCursor (source, property, tv, 0);
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::Assert(nsIRDFResource* source, nsIRDFResource* property, 
                           nsIRDFNode* target, PRBool tv) {
    Assertion next = getArg1(source);
    Assertion prev = next;
    Assertion as = 0;
    while (next) {
        if ((next->property == property) && (next->v == target)) {
            next->tv = tv;
            return NS_OK;
        }
        prev = next;
        next = as->next;
    }
    as = makeNewAssertion(source, property, target, tv);
    if (!prev) {
        setArg1(source, as);
    } else {
        prev->next = as;
    }
    
    prev = 0;
    for (next = getArg2(target); next != null; next = next->InvNext) {prev = next;}
    if (!prev) {
        setArg2(target, as);
    } else {
        prev->InvNext = as;
    }

    for (count = 0; count < mObserverCount ; count++) {
        mObservers[count]->OnAssert(source, property, target);
    }
    
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::Unassert(nsIRDFResource* source,
                             nsIRDFResource* property, nsIRDFNode* target) {
    Assertion next = getArg1(source);
    Assertion prev = next;
    Assertion as = 0;
    while (next) {
        if ((next->property == property) && (next->v == target)) {
            if (prev == next) {
                setArg1(source, next->next);
            } else {
                prev->next = next->next;
            }
            as = next;
            break;
        }
        prev = next;
        next = as->next;
    }
    if (!as) return NS_OK;
    next = prev = getArg2(target) ;
    while (next) {
        if (next == as) {
            if (prev == next) {
                setArg2(target, next->invNext);
            } else {
                prev->invNext = next->invNext;
            }
            foundp = 1;
        }
        prev = next;
        next = as->invNext;
    }
    for (count = 0; count < mObserverCount ; count++) {
        mObservers[count]->OnUnassert(source, property, target);
    }

    return NS_OK;
}


NS_IMETHOD 
InMemoryDataSource::AddObserver(nsIRDFObserver* observer) {
    if (mObserverCount >= mObserverArrayLength) {
        nsIRDFObserver** old = mObservers;
        mObservers = (nsIRDFObserver**) realloc(mObserver, mObserverArrayLength + 
                                                DEFAULT_OBSERVER_ARRAY_LENGTH);
        // garbage collect old
        mObserverArrayLength = mObserverArrayLength + DEFAULT_OBSERVER_ARRAY_LENGTH;
    }
    mObservers[mObserverCount++] = observer;
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::RemoveObserver(nsIRDFObserver* observer) {
    size_t n;
    for (n = 0; n < mObserverCount; n++) {
        if (mObservers[n] == observer) {
            // reduce reference count to observer?
            if (n == (mObserverCount -1)) {
                mObservers[n] = 0;
            } else {
                mObservers[n] = mObservers[mObserverCount-1];
            }
            mObserverCount--;            
        }
    }
    return NS_OK;
}

NS_IMETHOD 
InMemoryDataSource::ArcLabelsIn(nsIRDFNode* node, nsIRDFCursor** labels) {
    // XXX implement later
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHOD 
InMemoryDataSource::ArcLabelsOut(nsIRDFResource* source,nsIRDFCursor** labels) {
    // XXX implement later
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHOD 
InMemoryDataSource::Flush() {
    // XXX implement later
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}




////////////////////////////////////////////////////////////////////////
// InMemoryDataSourceCursor

class InMemoryDataSourceCursor : public nsIRDFCursor {
private:
    nsIRDFResource* mSource;
    nsIRDFResource* mLabel;
    nsIRDFNode* mTarget;
    nsIRDFNode* mValue;
    size_t       mCount;
    PRBool       mTv;
    PRBool       mInversep;
    Assertion    mNextAssertion;

public:
    InMemoryDataSourceCursor (nsIRDFNode* u, nsIRDFResource* s, PRBool tv, PRBool inversep);
    virtual ~InMemoryDataSourceCursor(void);

    NS_DECL_ISUPPORTS
   
    NS_IMETHOD LoadNext(PRBool *result);
    NS_IMETHOD CursorValue(nsIRDFNode** u);
};

InMemoryDataSourceCursor::InMemoryDataSourceCursor (nsIRDFNode* u, nsIRDFResource* label, 
                                                    PRBool tv, PRBool inversep) {
    NS_INIT_REFCNT();
    mInversep = inversep;
    if (inversep) {
        mTarget = u;
    } else {
        mSource = u;
    }

    mLabel  = label;
    mCount  = 0;
    mTv     = tv;
    mNextAssertion = (inversep ? getArg2(u) : getArg1(u));    
}

InMemoryDataSourceCursor::~InMemoryDataSourceCursor(void)
{
}

NS_IMPL_ISUPPORTS(InMemoryDataSourceCursor, kIRDFCursorIID);

NS_IMETHODIMP
InMemoryDataSourceCursor::LoadNext(PRBool* result)
{

    if (! result )
        return NS_ERROR_NULL_POINTER;

    if (!mNextAssertion) {
        *result = 0;
        return NS_OK;
    }

    while (mNextAssertion) {
        if ((mTv == mNextAssertion->tv) && (mNextAssertion->label == mLabel)) {
            if (mInversep) {
                mValue = mNextAssertion->u;
            } else {
                mValue = mNextAssertion->v;
            }
            *result = 1;
            return NS_OK;
        }
        mNextAssertion = (mInversep ? mNextAssertion->invNext : mNextAssertion->next);
    }
    
    *result = 0;
    return NS_OK;
}

NS_IMETHODIMP
InMemoryDataSourceCursor::CursorValue(nsIRDFNode** result) {    
{
    if (! result )
        return NS_ERROR_NULL_POINTER;

    *result = mValue;
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
