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

  1) There is no implementation for walking property arcs backwards;
     i.e., GetSource(), GetSources(), and ArcLabelsIn(). Gotta fix
     this. Or maybe factor the API so that you don't always need to
     implement that crap. I can see that being a real headache for
     some implementations.

  2) Need to hook up observer mechanisms.

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

typedef struct _AssertionStruct {
    nsIRDFResource* u;
    nsIRDFResource* s;
    nsIRDFNode*     v;
    PRBool          tv;
} AssertionStruct;

typedef AssertionStruct* Assertion;

////////////////////////////////////////////////////////////////////////

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}

////////////////////////////////////////////////////////////////////////



class InMemoryDataSource : public nsIRDFDataSource {
protected:
    char*        mURL;
    PLHashTable* mArg1;
    PLHashTable* mArg2;
    nsIRDFObserver** mObservers;  
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

MemoryDataSource::MemoryDataSource (const char* url) {
    mURL = copyString(url);
    mArg1 = PL_NewHashTable(500, rdf_HashPointer, PL_CompareValues,
                            PL_CompareValues,  NULL, NULL);
    mArg2 = PL_NewHashTable(500, rdf_HashPointer, PL_CompareValues,
                            PL_CompareValues,  NULL, NULL);
}

InMemoryDataSourceCursor::~InMemoryDataSourceCursor(void)
{
    PL_HashTableDestroy(mArg1);
    PL_HashTableDestroy(mArg2);
}

Assertion 
InMemoryDataSourceCursor::getArg1 (nsIRDFResource* u) {
    return PL_HashTableLookup(mArg1, u);
}

Assertion 
InMemoryDataSourceCursor::getArg2 (nsIRDFNode* v) {
    return PL_HashTableLookup(mArg2, u);
}   

nsresult  
InMemoryDataSourceCursor::setArg1 (nsIRDFResource* u, Assertion as) {
    PL_HashTableAdd(mArg1, u, as);
    return NS_OK;
}

nsresult  
InMemoryDataSourceCursor::setArg2 (nsIRDFNode* v, Assertion as) {
    PL_HashTableAdd(mArg2, u, as);
    return NS_OK;
}

NS_IMETHOD 
GetSource(nsIRDFResource* property, nsIRDFNode* target,
          PRBool tv, nsIRDFResource** source) {

}

NS_IMETHOD 
GetSources(nsIRDFResource* property,nsIRDFNode* target,
           PRBool tv, nsIRDFCursor** sources) {
}

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
// MemoryDataSourceImpl implementation

MemoryDataSourceImpl::MemoryDataSourceImpl(void)
{
    NS_INIT_REFCNT();
    mNodes = PL_NewHashTable(kNodeTableSize, rdf_HashPointer,
                             PL_CompareValues,
                             PL_CompareValues,
                             &gHashAllocOps, nsnull);

    PR_ASSERT(mNodes);
}



MemoryDataSourceImpl::~MemoryDataSourceImpl(void)
{
    PL_HashTableDestroy(mNodes);
    mNodes = nsnull;
}

NS_IMPL_ISUPPORTS(MemoryDataSourceImpl, kIRDFDataSourceIID);


NS_IMETHODIMP
MemoryDataSourceImpl::Init(const char* uri)
{
    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHODIMP
MemoryDataSourceImpl::GetSources(nsIRDFResource* property,
                                 nsIRDFNode* target,
                                 PRBool tv,
                                 nsIRDFCursor** sources)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHODIMP
MemoryDataSourceImpl::GetTarget(nsIRDFResource* source,
                                nsIRDFResource* property,
                                PRBool tv,
                                nsIRDFNode** target)
{
    NS_PRECONDITION(source && property && target, "null ptr");
    if (!source || !property || !target)
        return NS_ERROR_NULL_POINTER;

    *target = nsnull; // reasonable default

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_ERROR_FAILURE;

    *target = u->GetProperty(property, tv);
    if (! *target)
        return NS_ERROR_FAILURE;

    NS_ADDREF(*target); // need to AddRef()
    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::GetTargets(nsIRDFResource* source,
                                 nsIRDFResource* property,
                                 PRBool tv,
                                 nsIRDFCursor** targets)
{
    NS_PRECONDITION(source && property && targets, "null ptr");
    if (!source || !property || !targets)
        return NS_ERROR_NULL_POINTER;

    NS_NewEmptyRDFCursor(targets); // reasonable default

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_OK;

    if (! (*targets = u->GetProperties(property, tv)))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*targets);
    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::Assert(nsIRDFResource* source, 
                             nsIRDFResource* property, 
                             nsIRDFNode* target,
                             PRBool tv)
{
    NS_PRECONDITION(source && property && target, "null ptr");
    if (!source || !property || !target)
        return NS_ERROR_NULL_POINTER;

    NodeImpl* u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source));
    if (! u) {
        u = new NodeImpl(source);
        if (! u)
            return NS_ERROR_OUT_OF_MEMORY;

        PL_HashTableAdd(mNodes, source, u);
    }

    u->AddProperty(property, target, tv);

#ifdef DEBUG_waterson
    PRBool b = PR_FALSE;
    if (b)
        Dump();
#endif

    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::Unassert(nsIRDFResource* source,
                               nsIRDFResource* property,
                               nsIRDFNode* target)
{
    NS_PRECONDITION(source && property && target, "null ptr");
    if (!source || !property || !target)
        return NS_ERROR_NULL_POINTER;

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_OK;

    u->RemoveProperty(property, target);

    // XXX If that was the last property, we should remove the node from mNodes to
    // conserve space

#ifdef DEBUG_waterson
    PRBool b = PR_FALSE;
    if (b)
        Dump();
#endif

    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::HasAssertion(nsIRDFResource* source,
                                   nsIRDFResource* property,
                                   nsIRDFNode* target,
                                   PRBool tv,
                                   PRBool* hasAssertion)
{
    NS_PRECONDITION(source && property && target && hasAssertion, "null ptr");
    if (!source || !property || !target || !hasAssertion)
        return NS_ERROR_NULL_POINTER;

    *hasAssertion = PR_FALSE; // reasonable default

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_OK;

    *hasAssertion = (u->GetProperty(property, tv) != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::AddObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MemoryDataSourceImpl::RemoveObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MemoryDataSourceImpl::ArcLabelsIn(nsIRDFNode* node,
                                  nsIRDFCursor** labels)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MemoryDataSourceImpl::ArcLabelsOut(nsIRDFResource* source,
                                   nsIRDFCursor** labels)
{
    NS_PRECONDITION(source && labels, "null ptr");
    if (!source || !labels)
        return NS_ERROR_NULL_POINTER;

    NS_NewEmptyRDFCursor(labels); // reasonable default

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_OK;

    if (! (*labels = u->GetArcLabelsOut()))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*labels);
    return NS_OK;
}

NS_IMETHODIMP
MemoryDataSourceImpl::Flush()
{
    return NS_OK; // nothing to flush!
}


void* PR_CALLBACK
MemoryDataSourceImpl::AllocTable(void* pool, PRSize size)
{
    return new char[size];
}


void PR_CALLBACK
MemoryDataSourceImpl::FreeTable(void* pool, void* item)
{
    delete[] item;
}


PLHashEntry* PR_CALLBACK
MemoryDataSourceImpl::AllocEntry(void* pool, const void* key)
{
    return new PLHashEntry;
}


void PR_CALLBACK
MemoryDataSourceImpl::FreeEntry(void* poool, PLHashEntry* he, PRUintn flag)
{
    if (flag == HT_FREE_VALUE || flag == HT_FREE_ENTRY) {
        NodeImpl* node = NS_STATIC_CAST(NodeImpl*, he->value);
        delete node;
    }
}

static PRIntn
rdf_NodeDumpEnumerator(PLHashEntry* he, PRIntn index, void* closure)
{
    // XXX we're doomed once literals go into the table.
    nsIRDFResource* node =
        NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));

    const char* s;
    node->GetValue(&s);

    printf("[Node: %s]\n", s);

    NodeImpl* impl = NS_STATIC_CAST(NodeImpl*, he->value);
    impl->Dump(s);

    return HT_ENUMERATE_NEXT;
}

void
MemoryDataSourceImpl::Dump(void)
{
    PL_HashTableEnumerateEntries(mNodes, rdf_NodeDumpEnumerator, nsnull);
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFMemoryDataSource(nsIRDFDataSource** result)
{
    MemoryDataSourceImpl* ds = new MemoryDataSourceImpl();
    if (! ds)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}
