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


#include "nsIRDFNode.h"
#include "nsRDFResourceManager.h"
#include "nsString.h"
#include "nsIAtom.h"

static NS_DEFINE_IID(kIRDFResourceManagerIID, NS_IRDFRESOURCEMANAGER_IID);

////////////////////////////////////////////////////////////////////////

class RDFNodeImpl : public nsIRDFNode {
public:
    RDFNodeImpl(nsRDFResourceManager* mgr, nsIAtom* value);
    virtual ~RDFNodeImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_IMETHOD GetStringValue(nsString& value) const;

    NS_IMETHOD GetAtomValue(nsIAtom*& value) const;


private:
    nsIAtom*              mValue;
    nsRDFResourceManager* mMgr;
};


RDFNodeImpl::RDFNodeImpl(nsRDFResourceManager* mgr, nsIAtom* value)
    : mMgr(mgr), mValue(value)
{
    NS_INIT_REFCNT();
    NS_IF_ADDREF(mValue);
    NS_IF_ADDREF(mMgr);
}


RDFNodeImpl::~RDFNodeImpl(void)
{
    mMgr->ReleaseNode(this);
    NS_IF_RELEASE(mValue);
    NS_IF_RELEASE(mMgr);
}


static NS_DEFINE_IID(kIRDFNodeIID, NS_IRDFNODE_IID);
NS_IMPL_ISUPPORTS(RDFNodeImpl, kIRDFNodeIID);

NS_IMETHODIMP
RDFNodeImpl::GetStringValue(nsString& value) const
{
    mValue->ToString(value);
    return NS_OK;
}

NS_IMETHODIMP
RDFNodeImpl::GetAtomValue(nsIAtom*& value) const
{
    value = mValue;
    value->AddRef();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// ResourceHashKey

class ResourceHashKey : public nsHashKey
{
private:
    nsIAtom* mResource;

public:
    ResourceHashKey(nsIAtom* resource) : mResource(resource) {
        NS_IF_ADDREF(mResource);
    }

    virtual ~ResourceHashKey(void) {
        NS_IF_RELEASE(mResource);
    }

    // nsHashKey pure virtual interface methods
    virtual PRUint32 HashValue(void) const {
        return (PRUint32) mResource;
    }

    virtual PRBool Equals(const nsHashKey* aKey) const {
        ResourceHashKey* that;

        if (! (that = NS_DYNAMIC_CAST(ResourceHashKey*, aKey)))
            return PR_FALSE;

        return (that->mResource == this->mResource);
    }

    virtual nsHashKey* Clone(void) const {
        return new ResourceHashKey(mResource);
    }
};


////////////////////////////////////////////////////////////////////////
// nsRDFResourceManager

nsRDFResourceManager::nsRDFResourceManager(void)
{
    NS_INIT_REFCNT();
}


nsRDFResourceManager::~nsRDFResourceManager(void)
{
    // XXX LEAK! Make sure you release the nodes!
}


NS_IMPL_ISUPPORTS(nsRDFResourceManager, kIRDFResourceManagerIID);


NS_IMETHODIMP
nsRDFResourceManager::GetNode(const nsString& uri, nsIRDFNode*& resource)
{
    nsIAtom* atom = NS_NewAtom(uri);
    if (!atom)
        return NS_ERROR_OUT_OF_MEMORY;

    ResourceHashKey key(atom);

    resource = static_cast<nsIRDFNode*>(mResources.Get(&key));
    if (! resource) {
        resource = new RDFNodeImpl(this, atom);
        if (resource)
            mResources.Put(&key, resource);

        // We don't AddRef() the resource.
    }

    atom->Release();

    if (! resource)
        return NS_ERROR_OUT_OF_MEMORY;

    resource->AddRef();
    return NS_OK;
}


void
nsRDFResourceManager::ReleaseNode(const RDFNodeImpl* resource)
{
    nsIAtom* atom;
    resource->GetAtomValue(atom);
    ResourceHashKey key(atom);
    atom->Release();

    mResources.Remove(&key);
}
