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
#include "nsIRDFResourceFactory.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsRDFCID.h"
#include "plstr.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFNodeIID,            NS_IRDFNODE_IID);
static NS_DEFINE_IID(kIRDFResourceFactoryIID, NS_IRDFRESOURCEFACTORY_IID);
static NS_DEFINE_IID(kIRDFResourceIID,        NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID,         NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFServiceCID,          NS_RDFSERVICE_CID);

////////////////////////////////////////////////////////////////////////

class DefaultResourceImpl : public nsIRDFResource
{
public:
    DefaultResourceImpl(const char* uri);
    virtual ~DefaultResourceImpl(void);

    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIRDFNode
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result) const;

    // nsIRDFResource
    NS_IMETHOD GetValue(const char* *uri) const;
    NS_IMETHOD EqualsResource(const nsIRDFResource* resource, PRBool* result) const;
    NS_IMETHOD EqualsString(const char* uri, PRBool* result) const;

    // Implementation methods
    const char* GetURI(void) const {
        return mURI;
    }

private:
    char* mURI;
};


DefaultResourceImpl::DefaultResourceImpl(const char* uri)
{
    NS_INIT_REFCNT();
    mURI = PL_strdup(uri);
}


DefaultResourceImpl::~DefaultResourceImpl(void)
{
    nsresult rv;

    nsIRDFService* mgr;
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      kIRDFServiceIID,
                                      (nsISupports**) &mgr);

    PR_ASSERT(NS_SUCCEEDED(rv));
    if (NS_SUCCEEDED(rv)) {
        mgr->UnCacheResource(this);
        nsServiceManager::ReleaseService(kRDFServiceCID, mgr);
    }

    // N.B. that we need to free the URI *after* we un-cache the resource,
    // due to the way that the resource manager is implemented.
    PL_strfree(mURI);
}


NS_IMPL_ADDREF(DefaultResourceImpl);
NS_IMPL_RELEASE(DefaultResourceImpl);

nsresult
DefaultResourceImpl::QueryInterface(REFNSIID iid, void** result)
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
DefaultResourceImpl::EqualsNode(nsIRDFNode* node, PRBool* result) const
{
    nsresult rv;
    nsIRDFResource* resource;
    if (NS_SUCCEEDED(node->QueryInterface(kIRDFResourceIID, (void**) &resource))) {
        rv = EqualsResource(resource, result);
        NS_RELEASE(resource);
    }
    else {
        *result = PR_FALSE;
        rv = NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
DefaultResourceImpl::GetValue(const char* *uri) const
{
    if (!uri)
        return NS_ERROR_NULL_POINTER;

    *uri = mURI;
    return NS_OK;
}

NS_IMETHODIMP
DefaultResourceImpl::EqualsResource(const nsIRDFResource* resource, PRBool* result) const
{
    if (!resource || !result)
        return NS_ERROR_NULL_POINTER;

    *result = (resource == this);
    return NS_OK;
}


NS_IMETHODIMP
DefaultResourceImpl::EqualsString(const char* uri, PRBool* result) const
{
    if (!uri || !result)
        return NS_ERROR_NULL_POINTER;

    *result = (PL_strcmp(uri, mURI) == 0);
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////

class DefaultResourceFactoryImpl : public nsIRDFResourceFactory
{
public:
    DefaultResourceFactoryImpl(void);
    virtual ~DefaultResourceFactoryImpl(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateResource(const char* aURI, nsIRDFResource** aResult);
};


DefaultResourceFactoryImpl::DefaultResourceFactoryImpl(void)
{
    NS_INIT_REFCNT();
}


DefaultResourceFactoryImpl::~DefaultResourceFactoryImpl(void)
{
}


NS_IMPL_ISUPPORTS(DefaultResourceFactoryImpl, kIRDFResourceFactoryIID);


NS_IMETHODIMP
DefaultResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    DefaultResourceImpl* resource = new DefaultResourceImpl(aURI);
    if (! resource)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(resource);
    *aResult = resource;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFDefaultResourceFactory(nsIRDFResourceFactory** result)
{
    NS_PRECONDITION(result != nsnull, "null ptr");
    if (! result)
        return NS_ERROR_NULL_POINTER;

    DefaultResourceFactoryImpl* factory = new DefaultResourceFactoryImpl();
    if (! factory)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *result = factory;
    return NS_OK;
}
