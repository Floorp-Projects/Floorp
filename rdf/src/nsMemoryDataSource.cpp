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
#include "nsMemoryDataSource.h"
#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsIRDFRegistry.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"

extern nsIRDFCursor* gEmptyCursor;

static NS_DEFINE_IID(kIRDFCursorIID,     NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,       NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFRegistryIID,   NS_IRDFREGISTRY_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFRegistryCID,        NS_RDFREGISTRY_CID);

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

class NodeHashKey;

class NodeImpl {
protected:
    nsIRDFNode* mNode;
    nsHashtable mProperties; // maps an nsIRDFNode* property to a property list

    inline PropertyListElement* FindPropertyValue(NodeHashKey* key, nsIRDFNode* value, PRBool tv);

public:
    NodeImpl(nsIRDFNode* node) : mNode(node) {
        NS_IF_ADDREF(mNode);
    }

    virtual ~NodeImpl(void);

    nsIRDFNode* GetNode(void) {
        return mNode;
    }

    PRBool HasPropertyValue(nsIRDFNode* property, nsIRDFNode* value, PRBool tv);
    void AddProperty(nsIRDFNode* property, nsIRDFNode* value, PRBool tv);
    void RemoveProperty(nsIRDFNode* property, nsIRDFNode* value);
    nsIRDFNode* GetProperty(nsIRDFNode* property, PRBool tv);
    nsIRDFCursor* GetProperties(nsIRDFNode* property, PRBool tv);
    nsIRDFCursor* GetArcLabelsOut(void);
};

////////////////////////////////////////////////////////////////////////
// NodeHashKey

class NodeHashKey : public nsHashKey
{
private:
    nsIRDFNode* mNode;

public:
    NodeHashKey(void) : mNode(nsnull) {
    }

    NodeHashKey(nsIRDFNode* aNode) : mNode(aNode) {
        NS_IF_ADDREF(mNode);
    }

    NodeHashKey(const NodeHashKey& key) : mNode(key.mNode) {
        NS_IF_ADDREF(mNode);
    }

    NodeHashKey& operator=(const NodeHashKey& key) {
        NS_IF_RELEASE(mNode);
        mNode = key.mNode;
        NS_IF_ADDREF(mNode);
        return *this;
    }

    NodeHashKey& operator=(nsIRDFNode* aNode) {
        NS_IF_RELEASE(mNode);
        mNode = aNode;
        NS_IF_ADDREF(mNode);
        return *this;
    }

    virtual ~NodeHashKey(void) {
        NS_IF_RELEASE(mNode);
    }

    nsIRDFNode* GetNode(void) {
        return mNode;
    }

    // nsHashKey pure virtual interface methods
    virtual PRUint32 HashValue(void) const {
        return (PRUint32) mNode;
    }

    virtual PRBool Equals(const nsHashKey* aKey) const {
        NodeHashKey* that;

        // XXX like to do a dynamic_cast<> here.
        that = (NodeHashKey*) aKey;

        return (that->mNode == this->mNode);
    }

    virtual nsHashKey* Clone(void) const {
        return new NodeHashKey(mNode);
    }
};


////////////////////////////////////////////////////////////////////////
// PropertyCursorImpl

class PropertyCursorImpl : public nsIRDFCursor {
private:
    PropertyListElement* mFirst;
    PropertyListElement* mNext;
    PRBool               mTruthValueToMatch;

public:
    PropertyCursorImpl(PropertyListElement* first, PRBool truthValue);
    virtual ~PropertyCursorImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD HasMoreElements(PRBool& result);
    NS_IMETHOD GetNext(nsIRDFNode*& next, PRBool& tv);
};


PropertyCursorImpl::PropertyCursorImpl(PropertyListElement* first, PRBool truthValueToMatch)
    : mFirst(first),
      mNext(first),
      mTruthValueToMatch(truthValueToMatch)
{
    NS_INIT_REFCNT();
    while (1) {
        if (mNext->GetTruthValue() == mTruthValueToMatch)
            break;

        if (mNext == mFirst) {
            mNext = nsnull; // wrapped all the way back to the start
            break;
        }

        mNext = mNext->GetNext();
    }
}


PropertyCursorImpl::~PropertyCursorImpl(void)
{
}

NS_IMPL_ISUPPORTS(PropertyCursorImpl, kIRDFCursorIID);

NS_IMETHODIMP
PropertyCursorImpl::HasMoreElements(PRBool& result)
{
    result = (mNext != nsnull);
    return NS_OK;
}


NS_IMETHODIMP
PropertyCursorImpl::GetNext(nsIRDFNode*& next, PRBool& tv)
{
    if (mNext == nsnull)
        return NS_ERROR_UNEXPECTED;

    next = mNext->GetValue();
    tv   = mNext->GetTruthValue();

    mNext = mNext->GetNext(); // advance past the current node
    while (1) {
        if (mNext == mFirst) {
            mNext = nsnull; // wrapped all the way back to the start
            break;
        }

        if (mNext->GetTruthValue() == mTruthValueToMatch)
            break;

        mNext = mNext->GetNext();
    }

    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// ArcCursorImpl

class ArcCursorImpl : public nsIRDFCursor {
private:
    nsISupportsArray* mProperties;
    static PRBool Enumerator(nsHashKey* key, void* value, void* closure);

public:
    ArcCursorImpl(nsHashtable& properties);
    virtual ~ArcCursorImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD HasMoreElements(PRBool& result);
    NS_IMETHOD GetNext(nsIRDFNode*& next, PRBool& tv);
};


ArcCursorImpl::ArcCursorImpl(nsHashtable& properties)
{
    NS_INIT_REFCNT();
    if (NS_SUCCEEDED(NS_NewISupportsArray(&mProperties)))
        properties.Enumerate(Enumerator, mProperties);
}

ArcCursorImpl::~ArcCursorImpl(void)
{
    NS_IF_RELEASE(mProperties);
}

NS_IMPL_ISUPPORTS(ArcCursorImpl, kIRDFCursorIID);

PRBool
ArcCursorImpl::Enumerator(nsHashKey* key, void* value, void* closure)
{
    nsISupportsArray* properties = NS_STATIC_CAST(nsISupportsArray*, closure);
    NodeHashKey* k = (NodeHashKey*) key;
    properties->AppendElement(k->GetNode());
    return PR_TRUE;
}

NS_IMETHODIMP
ArcCursorImpl::HasMoreElements(PRBool& result)
{
    result = (mProperties && mProperties->Count() > 0);
    return NS_OK;
}


NS_IMETHODIMP
ArcCursorImpl::GetNext(nsIRDFNode*& next, PRBool& tv)
{
    if (! mProperties || ! mProperties->Count())
        return NS_ERROR_UNEXPECTED;

    PRInt32 index = mProperties->Count() - 1;
    nsISupports* obj = mProperties->ElementAt(index); // this'll AddRef()

    PR_ASSERT(obj);
    if (obj) {
        obj->QueryInterface(kIRDFNodeIID, (void**) &next);
        obj->Release();
    }

    mProperties->RemoveElementAt(index);

    tv = PR_TRUE; // not really applicable...
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// NodeImpl implementation

NodeImpl::~NodeImpl(void)
{
    NS_IF_RELEASE(mNode);

    // XXX LEAK! we need to make sure that the properties are
    // released!
}


PropertyListElement*
NodeImpl::FindPropertyValue(NodeHashKey* key, nsIRDFNode* value, PRBool tv)
{
    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, mProperties.Get(key));

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
NodeImpl::HasPropertyValue(nsIRDFNode* property, nsIRDFNode* value, PRBool tv)
{
    NodeHashKey key(property);
    return (FindPropertyValue(&key, value, tv) != nsnull);
}

nsIRDFNode*
NodeImpl::GetProperty(nsIRDFNode* property, PRBool tv)
{
    NodeHashKey key(property);
    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, mProperties.Get(&key));

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
NodeImpl::AddProperty(nsIRDFNode* property, nsIRDFNode* value, PRBool tv)
{
    NodeHashKey key(property);
    if (FindPropertyValue(&key, value, tv))
        return;

    // XXX so both positive and negative assertions can live in the
    // graph together. This seems wrong...

    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, mProperties.Get(&key));

    PropertyListElement* e
        = new PropertyListElement(value, tv);

    if (head) {
        e->InsertAfter(head);
    } else {
        mProperties.Put(&key, e);
    }
}


void
NodeImpl::RemoveProperty(nsIRDFNode* property, nsIRDFNode* value)
{
    NodeHashKey key(property);
    PropertyListElement* e;

    // this is really bizarre that two truth values can live in one
    // data source. I'm suspicious that this isn't quite right...

    e = FindPropertyValue(&key, value, PR_TRUE);
    if (e) {
        if (e->UnlinkAndTestIfEmpty())
            mProperties.Remove(&key);
        delete e;
    }

    e = FindPropertyValue(&key, value, PR_FALSE);
    if (e) {
        if (e->UnlinkAndTestIfEmpty())
            mProperties.Remove(&key);
        delete e;
    }
}


nsIRDFCursor*
NodeImpl::GetProperties(nsIRDFNode* property, PRBool tv)
{
    NodeHashKey key(property);
    PropertyListElement* head
        = NS_STATIC_CAST(PropertyListElement*, mProperties.Get(&key));

    return (head) ? (new PropertyCursorImpl(head, tv)) : gEmptyCursor;
}


nsIRDFCursor*
NodeImpl::GetArcLabelsOut(void)
{
    return new ArcCursorImpl(mProperties);
}

////////////////////////////////////////////////////////////////////////
// nsMemoryDataSource implementation

nsMemoryDataSource::nsMemoryDataSource(void)
    : mRegistry(nsnull)
{
    NS_INIT_REFCNT();
}



nsMemoryDataSource::~nsMemoryDataSource(void)
{
    // XXX LEAK! make sure that you release the nodes
    if (mRegistry) {
        mRegistry->Unregister(this);
        nsServiceManager::ReleaseService(kRDFRegistryCID, mRegistry);
        mRegistry = nsnull;
    }
}

NS_IMPL_ISUPPORTS(nsMemoryDataSource, kIRDFDataSourceIID);


NS_IMETHODIMP
nsMemoryDataSource::Init(const nsString& uri)
{
    nsresult rv;

    if (mRegistry)
        return NS_ERROR_ALREADY_INITIALIZED;

    if (NS_FAILED(rv = nsServiceManager::GetService(kRDFRegistryCID,
                                                    kIRDFRegistryIID,
                                                    (nsISupports**) &mRegistry)))
        return rv;

    return mRegistry->Register(uri, this);
}

NS_IMETHODIMP
nsMemoryDataSource::GetSource(nsIRDFNode* property,
                              nsIRDFNode* target,
                              PRBool tv,
                              nsIRDFNode*& source)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHODIMP
nsMemoryDataSource::GetSources(nsIRDFNode* property,
                               nsIRDFNode* target,
                               PRBool tv,
                               nsIRDFCursor*& sources)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_IMETHODIMP
nsMemoryDataSource::GetTarget(nsIRDFNode* source,
                              nsIRDFNode* property,
                              PRBool tv,
                              nsIRDFNode*& target)
{
    NS_PRECONDITION(source && property, "null ptr");
    if (!source || !property)
        return NS_ERROR_NULL_POINTER;

    target = nsnull; // reasonable default

    NodeHashKey key(source);
    NodeImpl *u;

    if (! (u = NS_STATIC_CAST(NodeImpl*, mNodes.Get(&key))))
        return NS_ERROR_FAILURE;

    target = u->GetProperty(property, tv);
    if (! target)
        return NS_ERROR_FAILURE;

    NS_ADDREF(target); // need to AddRef()
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryDataSource::GetTargets(nsIRDFNode* source,
                               nsIRDFNode* property,
                               PRBool tv,
                               nsIRDFCursor*& targets)
{
    NS_PRECONDITION(source && property, "null ptr");
    if (!source || !property)
        return NS_ERROR_NULL_POINTER;

    targets = gEmptyCursor; // reasonable default

    NodeHashKey key(source);
    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, mNodes.Get(&key))))
        return NS_OK;

    if (! (targets = u->GetProperties(property, tv)))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(targets);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryDataSource::Assert(nsIRDFNode* source, 
                           nsIRDFNode* property, 
                           nsIRDFNode* target,
                           PRBool tv)
{
    NS_PRECONDITION(source && property && target, "null ptr");
    if (!source || !property || !target)
        return NS_ERROR_NULL_POINTER;

    NodeImpl *u;

    if (! (u = Ensure(source)))
        return NS_ERROR_OUT_OF_MEMORY;

    if (! Ensure(property))
        return NS_ERROR_OUT_OF_MEMORY;

    if (! Ensure(target))
        return NS_ERROR_OUT_OF_MEMORY;

    u->AddProperty(property, target, tv);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryDataSource::Unassert(nsIRDFNode* source,
                             nsIRDFNode* property,
                             nsIRDFNode* target)
{
    NS_PRECONDITION(source && property && target, "null ptr");
    if (!source || !property || !target)
        return NS_ERROR_NULL_POINTER;

    NodeHashKey key(source);
    NodeImpl *u;

    if (! (u = NS_STATIC_CAST(NodeImpl*, mNodes.Get(&key))))
        return NS_OK;

    u->RemoveProperty(property, target);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryDataSource::HasAssertion(nsIRDFNode* source,
                                 nsIRDFNode* property,
                                 nsIRDFNode* target,
                                 PRBool tv,
                                 PRBool& hasAssertion)
{
    NS_PRECONDITION(source && property && target, "null ptr");
    if (!source || !property || !target)
        return NS_ERROR_NULL_POINTER;

    hasAssertion = PR_FALSE; // reasonable default

    NodeHashKey key(source);
    NodeImpl *u;

    if (! (u = NS_STATIC_CAST(NodeImpl*, mNodes.Get(&key))))
        return NS_OK;

    hasAssertion = (u->GetProperty(property, tv) != nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryDataSource::AddObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryDataSource::RemoveObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryDataSource::ArcLabelsIn(nsIRDFNode* node,
                                nsIRDFCursor*& labels)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMemoryDataSource::ArcLabelsOut(nsIRDFNode* source,
                                 nsIRDFCursor*& labels)
{
    NS_PRECONDITION(source, "null ptr");
    if (! source)
        return NS_ERROR_NULL_POINTER;

    labels = gEmptyCursor; // reasonable default

    NodeHashKey key(source);
    NodeImpl *u;
    if (! (u = NS_STATIC_CAST(NodeImpl*, mNodes.Get(&key))))
        return NS_OK;

    if (! (labels = u->GetArcLabelsOut()))
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(labels);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryDataSource::Flush()
{
    return NS_OK; // nothing to flush!
}



NodeImpl*
nsMemoryDataSource::Ensure(nsIRDFNode* node)
{
    // The NodeHashKey does an AddRef(), and is cloned for insertion
    // into the nsHashTable. This makes resource management really
    // easy, because when the nsHashtable is destroyed, all of the
    // keys are automatically deleted, which in turn does an
    // appropriate Release() on the nsIRDFNode.

    NodeHashKey key(node);
    NodeImpl* result = NS_STATIC_CAST(NodeImpl*, mNodes.Get(&key));
    if (! result) {
        result = new NodeImpl(node);
        if (result)
            mNodes.Put(&key, result);
    }
    return result;
}



////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFMemoryDataSource(nsIRDFDataSource** result)
{
    nsMemoryDataSource* ds = new nsMemoryDataSource();
    if (! ds)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = ds;
    NS_ADDREF(*result);
    return NS_OK;
}
