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
    plagiarized from Chris Waterson the Great. So if you find 
    a fault here... make sure you notify him as well. 

    -Gagan Saksena 03/25/99

*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPCID.h"
#include "nsHTTPHandlerFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"
#include "nsIProtocolHandler.h" // for NS_NETWORK_PROTOCOL_PROGID_PREFIX

static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kHTTPHandlerCID,      NS_HTTP_HANDLER_FACTORY_CID);

////////////////////////////////////////////////////////////////////////

nsHTTPHandlerFactory::nsHTTPHandlerFactory(const nsCID &aClass)
    : mClassID(aClass)
{
    NS_INIT_REFCNT();
}

nsHTTPHandlerFactory::~nsHTTPHandlerFactory()
{
}

NS_IMETHODIMP
nsHTTPHandlerFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = nsnull;

    if (aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsISupports*, this);
        AddRef();
        return NS_OK;
    } else if (aIID.Equals(kIFactoryIID)) {
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
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kHTTPHandlerCID)) {
        if (NS_FAILED(rv = NS_CreateOrGetHTTPHandler((nsIHTTPProtocolHandler**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
    return rv;
}

nsresult nsHTTPHandlerFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}
