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


#include "nsIAtom.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsString.h"
#include "plhash.h"
#include "plstr.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFResourceManagerIID, NS_IRDFRESOURCEMANAGER_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,         NS_IRDFLITERAL_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFNodeIID,            NS_IRDFNODE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////

class ResourceImpl;

class ResourceManagerImpl : public nsIRDFResourceManager {
protected:
    PLHashTable* mResources;
    virtual ~ResourceManagerImpl(void);

public:
    ResourceManagerImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFResourceManager
    NS_IMETHOD GetResource(const char* uri, nsIRDFResource** resource);
    NS_IMETHOD GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource);
    NS_IMETHOD GetLiteral(const PRUnichar* value, nsIRDFLiteral** literal);

    void ReleaseNode(const ResourceImpl* resource);
};


////////////////////////////////////////////////////////////////////////

class ResourceImpl : public nsIRDFResource {
public:
    ResourceImpl(ResourceManagerImpl* mgr, const char* uri);
    virtual ~ResourceImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFResource
    NS_IMETHOD GetValue(const char* *uri) const;
    NS_IMETHOD EqualsResource(const nsIRDFResource* resource, PRBool* result) const;
    NS_IMETHOD EqualsString(const char* uri, PRBool* result) const;

    // Implementation methods
    const char* GetURI(void) const {
        return mURI;
    }

private:
    char*                mURI;
    ResourceManagerImpl* mMgr;
};


ResourceImpl::ResourceImpl(ResourceManagerImpl* mgr, const char* uri)
    : mMgr(mgr)
{
    NS_INIT_REFCNT();
    NS_IF_ADDREF(mMgr);
    mURI = PL_strdup(uri);
}


ResourceImpl::~ResourceImpl(void)
{
    mMgr->ReleaseNode(this);
    PL_strfree(mURI);
    NS_IF_RELEASE(mMgr);
}


NS_IMPL_ADDREF(ResourceImpl);
NS_IMPL_RELEASE(ResourceImpl);

nsresult
ResourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFResourceIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFResource*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
ResourceImpl::GetValue(const char* *uri) const
{
    if (!uri)
        return NS_ERROR_NULL_POINTER;

    *uri = mURI;
    return NS_OK;
}

NS_IMETHODIMP
ResourceImpl::EqualsResource(const nsIRDFResource* resource, PRBool* result) const
{
    if (!resource || !result)
        return NS_ERROR_NULL_POINTER;

    *result = (resource == this);
    return NS_OK;
}


NS_IMETHODIMP
ResourceImpl::EqualsString(const char* uri, PRBool* result) const
{
    if (!uri || !result)
        return NS_ERROR_NULL_POINTER;

    *result = (PL_strcmp(uri, mURI) == 0);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
//

class LiteralImpl : public nsIRDFLiteral {
public:
    LiteralImpl(const PRUnichar* s);
    virtual ~LiteralImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFLiteral
    NS_IMETHOD GetValue(const PRUnichar* *value) const;
    NS_IMETHOD Equals(const nsIRDFLiteral* literal, PRBool* result) const;

private:
    nsAutoString mValue;
};


LiteralImpl::LiteralImpl(const PRUnichar* s)
    : mValue(s)
{
    NS_INIT_REFCNT();
}

LiteralImpl::~LiteralImpl(void)
{
}

NS_IMPL_ADDREF(LiteralImpl);
NS_IMPL_RELEASE(LiteralImpl);

nsresult
LiteralImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFLiteralIID) ||
        iid.Equals(kIRDFNodeIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFLiteral*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
LiteralImpl::GetValue(const PRUnichar* *value) const
{
    NS_ASSERTION(value, "null ptr");
    if (! value)
        return NS_ERROR_NULL_POINTER;

    *value = mValue.GetUnicode();
    return NS_OK;
}


NS_IMETHODIMP
LiteralImpl::Equals(const nsIRDFLiteral* literal, PRBool* result) const
{
    NS_ASSERTION(literal && result, "null ptr");
    if (!literal || !result)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    const PRUnichar* p;
    if (NS_FAILED(rv = literal->GetValue(&p)))
        return rv;

    nsAutoString s(p);

    *result = s.Equals(mValue);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// ResourceManagerImpl


ResourceManagerImpl::ResourceManagerImpl(void)
    : mResources(nsnull)
{
    NS_INIT_REFCNT();
    mResources = PL_NewHashTable(1023,              // nbuckets
                                 PL_HashString,     // hash fn
                                 PL_CompareStrings, // key compare fn
                                 PL_CompareValues,  // value compare fn
                                 nsnull, nsnull);   // alloc ops & priv
}


ResourceManagerImpl::~ResourceManagerImpl(void)
{
    PL_HashTableDestroy(mResources);
}


NS_IMPL_ISUPPORTS(ResourceManagerImpl, kIRDFResourceManagerIID);


NS_IMETHODIMP
ResourceManagerImpl::GetResource(const char* uri, nsIRDFResource** resource)
{
    ResourceImpl* result =
        NS_STATIC_CAST(ResourceImpl*, PL_HashTableLookup(mResources, uri));

    if (! result) {
        result = new ResourceImpl(this, uri);
        if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

        // This is a little trick to make storage more efficient. For
        // the "key" in the table, we'll use the string value that's
        // stored as a member variable of the ResourceImpl object.
        PL_HashTableAdd(mResources, result->GetURI(), result);

        // *We* don't AddRef() the resource, because the resource
        // AddRef()s *us*.
    }

    result->AddRef();
    *resource = result;

    return NS_OK;
}


NS_IMETHODIMP
ResourceManagerImpl::GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource)
{
    nsString s(uri);
    char* cstr = s.ToNewCString();
    nsresult rv = GetResource(cstr, resource);
    delete[] cstr;
    return rv;
}


NS_IMETHODIMP
ResourceManagerImpl::GetLiteral(const PRUnichar* uri, nsIRDFLiteral** literal)
{
    LiteralImpl* result = new LiteralImpl(uri);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *literal = result;
    NS_ADDREF(result);
    return NS_OK;
}


void
ResourceManagerImpl::ReleaseNode(const ResourceImpl* resource)
{
    PL_HashTableRemove(mResources, resource->GetURI());
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFResourceManager(nsIRDFResourceManager** mgr)
{
    ResourceManagerImpl* result = new ResourceManagerImpl();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    *mgr = result;
    NS_ADDREF(result);
    return NS_OK;
}
