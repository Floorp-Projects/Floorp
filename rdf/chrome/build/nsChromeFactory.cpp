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

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIChromeRegistry.h"
#include "nscore.h"
#include "rdf.h"
#ifdef NECKO
#include "nsChromeProtocolHandler.h"
#endif

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);
#ifdef NECKO
static NS_DEFINE_CID(kChromeProtocolHandlerCID, NS_CHROMEPROTOCOLHANDLER_CID);
#endif

static NS_IMETHODIMP
NS_ConstructChromeRegistry(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    NS_ASSERTION(aOuter == nsnull, "no aggregation");
    nsIChromeRegistry* chromeRegistry;
    rv = NS_NewChromeRegistry(&chromeRegistry);
    if (NS_FAILED(rv)) {
        NS_ERROR("Unable to construct chrome registry");
        return rv;
    }
    rv = chromeRegistry->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(chromeRegistry);
    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    NS_ASSERTION(aFactory != nsnull, "bad factory pointer");

    nsIGenericFactory* fact;
    if (aClass.Equals(kChromeRegistryCID)) {
        rv = NS_NewGenericFactory(&fact, NS_ConstructChromeRegistry);
    }
#ifdef NECKO
    else if (aClass.Equals(kChromeProtocolHandlerCID)) {
        rv = NS_NewGenericFactory(&fact, nsChromeProtocolHandler::Create);
    }
#endif
    else {
        rv = NS_ERROR_FAILURE;
    }

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

    rv = compMgr->RegisterComponent(kChromeRegistryCID,
                                    "Chrome Registry",
                                    NS_RDF_DATASOURCE_PROGID_PREFIX "chrome",
                                    aPath,
                                    PR_TRUE, PR_TRUE);
#ifdef NECKO
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kChromeProtocolHandlerCID,  
                                    "Chrome Protocol Handler",
                                    NS_NETWORK_PROTOCOL_PROGID_PREFIX "chrome",
                                    aPath, PR_TRUE, PR_TRUE);
#endif
    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kChromeRegistryCID, aPath);
#ifdef NECKO
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kChromeProtocolHandlerCID, aPath);
#endif
    return rv;
}

