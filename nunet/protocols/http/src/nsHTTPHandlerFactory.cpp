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
    The nsHTTPHandlerFactory implementation. This was directly
    plagiarized from Chris Waterson the Great's nsRDFFactory.cpp
    So if you find a fault here... make sure you notify him as 
    well. 

    -Gagan Saksena 03/25/99
*/

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsHTTPHandlerFactory.h"
#include "nsHTTPCID.h"
#include "nsIHTTPHandler.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kHTTPHandlerFactoryCID, NS_HTTP_HANDLER_FACTORY_CID);

nsHTTPHandlerFactory::nsHTTPHandlerFactory(const nsCID &aClass, 
                               const char* className,
                               const char* progID)
    : mClassID(aClass), 
    mClassName(className), 
    mProgID(progID)
{
    NS_INIT_REFCNT();
}

nsHTTPHandlerFactory::~nsHTTPHandlerFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
nsHTTPHandlerFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = nsnull;

    if (aIID.Equals(nsISupports::GetIID())) {
        *aResult = NS_STATIC_CAST(nsISupports*, this);
        AddRef();
        return NS_OK;
    } else if (aIID.Equals(nsIFactory::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIFactory*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsHTTPHandlerFactory);
NS_IMPL_RELEASE(nsHTTPHandlerFactory);

NS_IMETHODIMP
nsHTTPHandlerFactory::CreateInstance(nsISupports *aOuter,
                               const nsIID &aIID,
                               void **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;
    nsresult rv;
    nsISupports *inst = nsnull;
    if (mClassID.Equals(nsIProtocolHandler::GetIID())) {
        if (NS_FAILED(rv = CreateOrGetHTTPHandler((nsIHTTPHandler**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface.
        NS_ERROR("don't support the interface you want");
    }
    NS_IF_RELEASE(inst);
    return rv;
}

nsresult nsHTTPHandlerFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// The actual C functions for factory compliance.

// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    nsHTTPHandlerFactory* factory = new nsHTTPHandlerFactory(aClass, aClassName, aProgID);
    if (factory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) 
        return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) 
        return rv;

    // register the factory
    rv = compMgr->RegisterComponent(
                            nsIProtocolHandler::GetIID(),  
                            "HTTP Handler",
                            NS_COMPONENT_NETSCAPE_NETWORK_PROTOCOLS "http",
                            aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) 
        (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    
    return rv;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) 
        return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) 
        return rv;

    rv = compMgr->UnregisterComponent(kHTTPHandlerFactoryCID,  aPath);
    if (NS_FAILED(rv))
        (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);

    return rv;
}




