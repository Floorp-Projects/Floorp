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
#include "nsRDFBaseCID.h"
#include "nsString.h"
#include "rdfutil.h"
#include "plhash.h"

static NS_DEFINE_IID(kIRDFCursorIID,     NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,    NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFNodeIID,       NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceIID,   NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////

static PLHashNumber
rdf_HashPointer(const void* key)
{
    return (PLHashNumber) key;
}

////////////////////////////////////////////////////////////////////////

class NodeImpl;

class MemoryDataSourceImpl : public nsIRDFDataSource {
protected:
    PLHashTable* mNodes;

    // hash table routines
    static void * PR_CALLBACK
    AllocTable(void *pool, PRSize size);

    static void PR_CALLBACK
    FreeTable(void *pool, void *item);

    static PLHashEntry * PR_CALLBACK
    AllocEntry(void *pool, const void *key);

    static void PR_CALLBACK
    FreeEntry(void *pool, PLHashEntry *he, PRUintn flag);

    static PLHashAllocOps gHashAllocOps;

    static const PRInt32 kNodeTableSize;

    void Dump(void);

public:
    MemoryDataSourceImpl(void);
    virtual ~MemoryDataSourceImpl(void);

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
};

PLHashAllocOps MemoryDataSourceImpl::gHashAllocOps = {
    MemoryDataSourceImpl::AllocTable, MemoryDataSourceImpl::FreeTable,
    MemoryDataSourceImpl::AllocEntry, MemoryDataSourceImpl::FreeEntry
};

const PRInt32 MemoryDataSourceImpl::kNodeTableSize = 101;


////////////////////////////////////////////////////////////////////////
// PropertyListElement

class PropertyListElement {
private:
    nsIRDFNode*          mValue;
    PRBool               mTruthValue;
    PropertyListElement* mNext;
    PropertyListElement* mPrev;

public:
    PropertyListElement(nsIRDFNode* value, PRBool truthValue)
        : mValue(value), mTruthValue(truthValue) {
        mNext = mPrev = this;
        NS_IF_ADDREF(mValue);
    }

    ~PropertyListElement(void) {
        NS_IF_RELEASE(mValue);
    }

    nsIRDFNode* GetValue(void) {
        return mValue;
    }

    PRBool GetTruthValue(void) {
        return mTruthValue;
    }

    PropertyListElement* GetNext(void) {
        return mNext;
    }

    PropertyListElement* GetPrev(void) {
        return mPrev;
    }

    void InsertAfter(PropertyListElement* e) {
        e->mNext->mPrev = this;
        this->mNext = e->mNext;
        e->mNext = this;
        this->mPrev = e;
    }

    PRBool UnlinkAndTestIfEmpty(void) {
        if (mPrev == mNext)
            return PR_TRUE;

        mPrev->mNext = mNext;
        mNext->mPrev = mPrev;
        return PR_FALSE;
    }
};



////////////////////////////////////////////////////////////////////////
// NodeImpl

class NodeImpl {
protected:
    nsIRDFNode* mNode;

    // hash table routines
    static void * PR_CALLBACK
    AllocTable(void *pool, PRSize size);

    static void PR_CALLBACK
    FreeTable(void *pool, void *item);

    static PLHashEntry * PR_CALLBACK
    AllocEntry(void *pool, const void *key);

    static void PR_CALLBACK
    FreeEntry(void *pool, PLHashEntry *he, PRUintn flag);

    static PLHashAllocOps gHashAllocOps;

    static const PRInt32 kPropertyTableSize;

    /** Maps an nsIRDFNode* property to a property list. Lazily instantiated */
    PLHashTable* mProperties;

    inline PropertyListElement*
    FindPropertyValue(nsIRDFResource* property, nsIRDFNode* value, PRBool tv);

public:
    NodeImpl(nsIRDFNode* node);

    virtual ~NodeImpl(void);

    nsIRDFNode* GetNode(void) {
        return mNode;
    }

    PRBool HasPropertyValue(nsIRDFResource* property, nsIRDFNode* value, PRBool tv);
    void AddProperty(nsIRDFResource* property, nsIRDFNode* value, PRBool tv);
    void RemoveProperty(nsIRDFResource* property, nsIRDFNode* value);
    nsIRDFNode* GetProperty(nsIRDFResource* property, PRBool tv);
    nsIRDFAssertionCursor* GetProperties(nsIRDFDataSource* ds, nsIRDFResource* property, PRBool tv);
    nsIRDFArcsOutCursor* GetArcsOut(nsIRDFDataSource* ds);

    void Dump(const char* resource);
};

PLHashAllocOps NodeImpl::gHashAllocOps = {
    NodeImpl::AllocTable, NodeImpl::FreeTable,
    NodeImpl::AllocEntry, NodeImpl::FreeEntry
};

// The initial size of the property hashtable
const PRInt32 NodeImpl::kPropertyTableSize = 7;

////////////////////////////////////////////////////////////////////////
// PropertyCursorImpl


// XXX This could really get screwed up if the graph mutates under the
// cursor. Problem is, the NodeImpl isn't refcounted.

class PropertyCursorImpl : public nsIRDFAssertionCursor {
private:
    PropertyListElement* mFirst;
    PropertyListElement* mNext;
    nsIRDFDataSource*    mDataSource;
    nsIRDFResource*      mSubject;
    nsIRDFResource*      mPredicate;
    PRBool               mTruthValue;

public:
    PropertyCursorImpl(PropertyListElement* first,
                       nsIRDFDataSource* ds,
                       nsIRDFResource* subject,
                       nsIRDFResource* predicate,
                       PRBool tv);

    virtual ~PropertyCursorImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD Advance(void);

    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aResource);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetObject(nsIRDFNode** aObject);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};


PropertyCursorImpl::PropertyCursorImpl(PropertyListElement* first,
                                       nsIRDFDataSource* ds,
                                       nsIRDFResource* subject,
                                       nsIRDFResource* predicate,
                                       PRBool tv)
    : mFirst(first),
      mNext(nsnull),
      mDataSource(ds),
      mSubject(subject),
      mPredicate(predicate),
      mTruthValue(tv)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDataSource);
    NS_ADDREF(mSubject);
    NS_ADDREF(mPredicate);
}


PropertyCursorImpl::~PropertyCursorImpl(void)
{
    NS_RELEASE(mPredicate);
    NS_RELEASE(mSubject);
    NS_RELEASE(mDataSource);
}

NS_IMPL_ISUPPORTS(PropertyCursorImpl, kIRDFCursorIID);

NS_IMETHODIMP
PropertyCursorImpl::Advance(void)
{
    if (! mNext) {
        mNext = mFirst;
        
        do {
            if (mNext->GetTruthValue() == mTruthValue)
                return NS_OK;

            mNext = mNext->GetNext();
        } while (mNext != mFirst);
    }
    else {
        mNext = mNext->GetNext();

        while (mNext != mFirst) {
            if (mNext->GetTruthValue() == mTruthValue)
                return NS_OK;

            mNext = mNext->GetNext();
        }
    }

    return NS_ERROR_RDF_CURSOR_EMPTY;
}


NS_IMETHODIMP
PropertyCursorImpl::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_ADDREF(mDataSource);
    *aDataSource = mDataSource;
    return NS_OK;
}


NS_IMETHODIMP
PropertyCursorImpl::GetSubject(nsIRDFResource** aSubject)
{
    NS_ADDREF(mSubject);
    *aSubject = mSubject;
    return NS_OK;
}


NS_IMETHODIMP
PropertyCursorImpl::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_ADDREF(mPredicate);
    *aPredicate = mPredicate;
    return NS_OK;
}


NS_IMETHODIMP
PropertyCursorImpl::GetObject(nsIRDFNode** aObject)
{
    nsIRDFNode* result = mNext->GetValue();
    NS_ADDREF(result);
    *aObject = result;
    return NS_OK;
}


NS_IMETHODIMP
PropertyCursorImpl::GetTruthValue(PRBool* aTruthValue)
{
    *aTruthValue = mTruthValue;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// ArcsOutCursorImpl

class ArcsOutCursorImpl : public nsIRDFArcsOutCursor {
private:
    nsIRDFDataSource* mDataSource;
    nsIRDFResource*   mSubject;
    nsISupportsArray* mProperties;
    nsIRDFResource*   mCurrent;

    static PRIntn Enumerator(PLHashEntry* he, PRIntn index, void* closure);

public:
    ArcsOutCursorImpl(nsIRDFDataSource* ds,
                      nsIRDFResource* subject,
                      PLHashTable* properties);

    virtual ~ArcsOutCursorImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD Advance(void);
    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource);
    NS_IMETHOD GetSubject(nsIRDFResource** aSubject);
    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate);
    NS_IMETHOD GetTruthValue(PRBool* aTruthValue);
};


ArcsOutCursorImpl::ArcsOutCursorImpl(nsIRDFDataSource* ds,
                                     nsIRDFResource* subject,
                                     PLHashTable* properties)
    : mDataSource(ds),
      mSubject(subject),
      mCurrent(nsnull)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mDataSource);
    NS_ADDREF(mSubject);

    // XXX this is pretty wasteful
    if (NS_SUCCEEDED(NS_NewISupportsArray(&mProperties)))
        PL_HashTableEnumerateEntries(properties, Enumerator, mProperties);
}

ArcsOutCursorImpl::~ArcsOutCursorImpl(void)
{
    NS_IF_RELEASE(mCurrent);
    NS_IF_RELEASE(mProperties);
    NS_RELEASE(mSubject);
    NS_RELEASE(mDataSource);
}

NS_IMPL_ISUPPORTS(ArcsOutCursorImpl, kIRDFCursorIID);

PRBool
ArcsOutCursorImpl::Enumerator(PLHashEntry* he, PRIntn index, void* closure)
{
    nsISupportsArray* properties = NS_STATIC_CAST(nsISupportsArray*, closure);
    nsIRDFResource* property = NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));
    properties->AppendElement(property);
    return HT_ENUMERATE_NEXT;
}

NS_IMETHODIMP
ArcsOutCursorImpl::Advance(void)
{
    if (! mProperties || ! mProperties->Count())
        return NS_ERROR_RDF_CURSOR_EMPTY;

    PRInt32 index = mProperties->Count() - 1;
    if (index < 0)
        return NS_ERROR_RDF_CURSOR_EMPTY;

    NS_IF_RELEASE(mCurrent);

    // this'll AddRef()...
    mCurrent = NS_STATIC_CAST(nsIRDFResource*, mProperties->ElementAt(index));

    mProperties->RemoveElementAt(index);
    return NS_OK;
}


NS_IMETHODIMP
ArcsOutCursorImpl::GetDataSource(nsIRDFDataSource** aDataSource)
{
    NS_ADDREF(mDataSource);
    *aDataSource = mDataSource;
    return NS_OK;
}

NS_IMETHODIMP
ArcsOutCursorImpl::GetSubject(nsIRDFResource** aSubject)
{
    NS_ADDREF(mSubject);
    *aSubject = mSubject;
    return NS_OK;
}

NS_IMETHODIMP
ArcsOutCursorImpl::GetPredicate(nsIRDFResource** aPredicate)
{
    NS_ADDREF(mCurrent);
    *aPredicate = mCurrent;
    return NS_OK;
}

NS_IMETHODIMP
ArcsOutCursorImpl::GetTruthValue(PRBool* aTruthValue)
{
    *aTruthValue = PR_TRUE; // XXX
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// NodeImpl implementation

NodeImpl::NodeImpl(nsIRDFNode* node)
    : mNode(node),
      mProperties(nsnull)
{
    NS_IF_ADDREF(mNode);
}


NodeImpl::~NodeImpl(void)
{
    NS_IF_RELEASE(mNode);

    if (mProperties) {
        // XXX LEAK! we need to make sure that the properties are
        // released!
        PL_HashTableDestroy(mProperties);
        mProperties = nsnull;
    }
}


PropertyListElement*
NodeImpl::FindPropertyValue(nsIRDFResource* property, nsIRDFNode* value, PRBool tv)
{
    if (! mProperties)
        return nsnull;

    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, PL_HashTableLookup(mProperties, property));

    if (! head)
        return nsnull;

    PropertyListElement* e = head;
    do {
        if (e->GetValue() == value && e->GetTruthValue() == tv)
            return e;

        e = e->GetNext();
    } while (e != head);

    return nsnull;
}


PRBool
NodeImpl::HasPropertyValue(nsIRDFResource* property, nsIRDFNode* value, PRBool tv)
{
    return (FindPropertyValue(property, value, tv) != nsnull);
}

nsIRDFNode*
NodeImpl::GetProperty(nsIRDFResource* property, PRBool tv)
{
    if (! mProperties)
        return nsnull;

    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, PL_HashTableLookup(mProperties, property));

    if (! head)
        return nsnull;

    PropertyListElement* e = head;
    do {
        if (e->GetTruthValue() == tv)
            return e->GetValue();

        e = e->GetNext();
    } while (e != head);

    return nsnull;
}

void
NodeImpl::AddProperty(nsIRDFResource* property, nsIRDFNode* value, PRBool tv)
{
    // don't allow dups
    if (FindPropertyValue(property, value, tv))
        return;

    // XXX so both positive and negative assertions can live in the
    // graph together. This seems wrong...

    if (! mProperties) {
        // XXX we'll need a custom allocator to free the linked
        // list. It leaks right now.
        mProperties = PL_NewHashTable(kPropertyTableSize,
                                      rdf_HashPointer,
                                      PL_CompareValues,
                                      PL_CompareValues,
                                      &gHashAllocOps, nsnull);

        PR_ASSERT(mProperties);
        if (! mProperties)
            return;
    }

    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, PL_HashTableLookup(mProperties, property));

    PropertyListElement* e
        = new PropertyListElement(value, tv);

    if (head) {
        e->InsertAfter(head);
    } else {
        PL_HashTableAdd(mProperties, property, e);
    }
}


void
NodeImpl::RemoveProperty(nsIRDFResource* property, nsIRDFNode* value)
{
    if (! mProperties)
        return;

    PropertyListElement* e;

    // this is really bizarre that two truth values can live in one
    // data source. I'm suspicious that this isn't quite right...

    e = FindPropertyValue(property, value, PR_TRUE);
    if (e) {
        if (e->UnlinkAndTestIfEmpty()) {
            PL_HashTableRemove(mProperties, property);
        }
        else {
            delete e;
        }
    }

    e = FindPropertyValue(property, value, PR_FALSE);
    if (e) {
        if (e->UnlinkAndTestIfEmpty()) {
            PL_HashTableRemove(mProperties, property);
        }
        else {
            delete e;
        }
    }
}


nsIRDFAssertionCursor*
NodeImpl::GetProperties(nsIRDFDataSource* ds, nsIRDFResource* property, PRBool tv)
{
    if (! mProperties)
        return nsnull;

    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, PL_HashTableLookup(mProperties, property));

    nsIRDFAssertionCursor* result = nsnull;
    if (head) {
        nsIRDFResource* resource;
        if (NS_SUCCEEDED(mNode->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
            result = new PropertyCursorImpl(head, ds, resource, property, tv);
            NS_RELEASE(resource);
        }
    }

    if (! result)
        NS_NewEmptyRDFAssertionCursor(&result);

    return result;
}


nsIRDFArcsOutCursor*
NodeImpl::GetArcsOut(nsIRDFDataSource* ds)
{
    nsIRDFArcsOutCursor* result;

    nsIRDFResource* resource;
    if (NS_SUCCEEDED(mNode->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        result = new ArcsOutCursorImpl(ds, resource, mProperties);
        NS_RELEASE(resource);
    }

    if (! result)
        NS_NewEmptyRDFArcsOutCursor(&result);

    return result;
}


void* PR_CALLBACK
NodeImpl::AllocTable(void* pool, PRSize size)
{
    return new char[size];
}


void PR_CALLBACK
NodeImpl::FreeTable(void* pool, void* item)
{
    delete[] item;
}


PLHashEntry* PR_CALLBACK
NodeImpl::AllocEntry(void* pool, const void* key)
{
    nsIRDFResource* property =
        NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, key));
    NS_ADDREF(property);
    return new PLHashEntry;
}


void PR_CALLBACK
NodeImpl::FreeEntry(void* poool, PLHashEntry* he, PRUintn flag)
{
    if (flag == HT_FREE_VALUE || flag == HT_FREE_ENTRY) {
        PropertyListElement* head = NS_STATIC_CAST(PropertyListElement*, he->value);
        PRBool empty;
        do {
            PropertyListElement* next = head->GetNext();
            empty = head->UnlinkAndTestIfEmpty();
            delete head;
            head = next;
        } while (! empty);
    }
    if (flag == HT_FREE_ENTRY) {
        nsIRDFResource* property =
            NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));

        NS_RELEASE(property);
        delete he;
    }
}

static PRIntn
rdf_PropertyDumpEnumerator(PLHashEntry* he, PRIntn index, void* closure)
{
    const char* u = (const char*) closure;
    nsIRDFResource* property =
        NS_CONST_CAST(nsIRDFResource*, NS_STATIC_CAST(const nsIRDFResource*, he->key));

    const char* s;
    property->GetValue(&s);

    PropertyListElement* head =
        NS_STATIC_CAST(PropertyListElement*, he->value);

    PropertyListElement* next = head;
    PR_ASSERT(next);

    do {
        nsIRDFNode* node = next->GetValue();
        nsIRDFResource* resource;
        nsIRDFLiteral* literal;

        if (NS_SUCCEEDED(node->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
            const char* v;
            resource->GetValue(&v);
            NS_RELEASE(resource);

            printf("  (%s %s %s)\n", u, s, v);
        }
        else if (NS_SUCCEEDED(node->QueryInterface(kIRDFLiteralIID, (void**) &literal))) {
            const PRUnichar* v;
            literal->GetValue(&v);
            NS_RELEASE(literal);

            nsAutoString str = v;
            char buf[1024];
            printf("  (%s %s %s)\n", u, s, str.ToCString(buf, sizeof buf));
        }
        next = next->GetNext();
    } while (next != head);

    return HT_ENUMERATE_NEXT;
}

void
NodeImpl::Dump(const char* resource)
{
    if (! mProperties)
        return;

    PL_HashTableEnumerateEntries(mProperties, rdf_PropertyDumpEnumerator, (void*) resource);
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
                                 nsIRDFAssertionCursor** sources)
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
                                 nsIRDFAssertionCursor** targets)
{
    NS_PRECONDITION(source && property && targets, "null ptr");
    if (!source || !property || !targets)
        return NS_ERROR_NULL_POINTER;

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_OK;

    if (! (*targets = u->GetProperties(this, property, tv)))
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
                                  nsIRDFArcsInCursor** labels)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MemoryDataSourceImpl::ArcLabelsOut(nsIRDFResource* source,
                                   nsIRDFArcsOutCursor** labels)
{
    NS_PRECONDITION(source && labels, "null ptr");
    if (!source || !labels)
        return NS_ERROR_NULL_POINTER;

    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, PL_HashTableLookup(mNodes, source))))
        return NS_OK;

    if (! (*labels = u->GetArcsOut(this)))
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
