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

  The default resource factory implementation. This resource factory
  produces nsIRDFResource objects for any URI prefix that is not
  covered by some other factory.

 */

#include "nsRDFResource.h"

#if 0

#include "nsIRDFResourceFactory.h"

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


NS_IMPL_ISUPPORTS(DefaultResourceFactoryImpl, nsIRDFResourceFactory::IID());


NS_IMETHODIMP
DefaultResourceFactoryImpl::CreateResource(const char* aURI, nsIRDFResource** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsRDFResource* resource = new nsRDFResource(aURI);
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

#endif

nsresult
NS_NewDefaultResource(nsIRDFResource** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsRDFResource* resource = new nsRDFResource();
    if (! resource)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(resource);
    *aResult = resource;
    return NS_OK;
}
