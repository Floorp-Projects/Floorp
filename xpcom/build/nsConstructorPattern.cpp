/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// nsIFoo.h
////////////////////////////////////////////////////////////////////////////////
// Here's an interface that we're going to work with. Normally it would be
// defined in nsIFoo.h:

#define NS_IFOO_IID                                  \
{ /* f981ba10-1547-11d3-9337-00104ba0fd40 */         \
    0xf981ba10,                                      \
    0x1547,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_FOO_CID                                   \
{ /* fe1a5870-1547-11d3-9337-00104ba0fd40 */         \
    0xfe1a5870,                                      \
    0x1547,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIFoo : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFOO_IID);

    NS_IMETHOD Init(const char* name) = 0;
    NS_IMETHOD Doit(int x) = 0;
};

// Define a global constructor function if you really need one. Otherwise
// you can always use nsIComponentManager::CreateInstance.
PR_EXTERN(nsresult)
NS_NewFoo(nsIFoo* *result, const char* name);

////////////////////////////////////////////////////////////////////////////////
// nsFoo.h
////////////////////////////////////////////////////////////////////////////////
// Here's the implementation class definition. Put this in an nsFoo.h (and don't
// export it to dist).

//#include "nsIFoo.h"

class nsFoo : public nsIFoo {
public:
    // Define the constructor, but don't give it arguments that are needed
    // for initialization that can fail:
    nsFoo();
    // Always make the destructor virtual: 
    virtual ~nsFoo();

    // Define a Create method to be used with a factory:
    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    // from nsIFoo:
    NS_IMETHOD Init(const char* name);

    NS_IMETHOD Doit(int x);

    // from nsISupports:
    NS_DECL_ISUPPORTS

protected:
    // instance variables:
    char* mName;
};

////////////////////////////////////////////////////////////////////////////////
// nsFoo.cpp
////////////////////////////////////////////////////////////////////////////////
// Here's the implementation. Put this in an nsFoo.cpp.

//#include "nsFoo.h"
#include "nsCRT.h"
#include <stdio.h>

// Don't do anything in the constructor that can fail. Just initialize all 
// the instance variables and get out:
nsFoo::nsFoo()
    : mName(NULL)
{
    NS_INIT_REFCNT();
}

// Do anything that can fail in the Init method. It's a good idea to provide
// one so that in the future if someone adds something that can fail they'll
// be encouraged to do it right:
NS_IMETHODIMP
nsFoo::Init(const char* name)
{
    mName = nsCRT::strdup(name);
    if (mName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

// Destroy and release things as usual in the destructor:
nsFoo::~nsFoo()
{
    if (mName)
        nsCRT::free(mName);
}

// Provide a Create method that can be called by a factory constructor:
NS_METHOD
nsFoo::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
  
    nsFoo* foo = new nsFoo();
    if (foo == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    // Note that Create doesn't initialize the instance -- that has to
    // be done by the caller since the initialization args aren't passed
    // in here.

    // AddRef before calling QI -- this makes it easier to handle the QI
    // failure case because we'll always just Release and return
    NS_ADDREF(foo);
    nsresult rv = foo->QueryInterface(aIID, aResult);

    // This will free it if QI failed:
    NS_RELEASE(foo);
    return rv;
}

// Implement the nsISupports methods:
NS_IMPL_ISUPPORTS(nsFoo, nsIFoo::GetIID());

// Implement the method(s) of the interface:
NS_IMETHODIMP
nsFoo::Doit(int x)
{
    printf("%s %d\n", mName, x);
    return NS_OK;
}

// Finally, (if you really need one) implement a global constructor function
// for this class:
PR_IMPLEMENT(nsresult)
NS_NewFoo(nsIFoo* *result, const char* name)
{
    // Use the constructor method to create this. Don't duplicate work.
    nsresult rv = nsFoo::Create(NULL, nsIFoo::GetIID(),
                                (void**)result);
    if (NS_SUCCEEDED(rv)) {
        // Only initialize if the constructor succeeded:
        rv = (*result)->Init(name);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsFooModuleFactory.cpp
////////////////////////////////////////////////////////////////////////////////
// This file defines the factory that instantiates nsFoo and any other classes
// in the same component. It also defines the component's NSGetFactory, 
// NSRegisterSelf and NSUnregisterSelf entry points:

// Include nsFoo.h to get the nsFoo::Create method:
//#include "nsFoo.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

// Include the generic factory stuff. It's the easiest and smallest way to 
// work with factories:
#include "nsGenericFactory.h"

static NS_DEFINE_CID(kFooCID, NS_FOO_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    nsIGenericFactory* fact;
    if (aClass.Equals(kFooCID))
        rv = NS_NewGenericFactory(&fact, nsFoo::Create);
    else 
        rv = NS_ERROR_FAILURE;

    if (NS_SUCCEEDED(rv))
        *aFactory = fact;
    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kFooCID, "Foo", NULL,
                                    aPath, PR_TRUE, PR_TRUE);

    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kFooCID, aPath);

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
