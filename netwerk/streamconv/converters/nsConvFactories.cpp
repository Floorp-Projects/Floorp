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

#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIRegistry.h"

#include "nsFTPDirListingConv.h"
#include "nsMultiMixedConv.h"

static NS_DEFINE_CID(kComponentManagerCID,       NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kRegistryCID,               NS_REGISTRY_CID);


////////////////////////////////////////////////////////////////////////////////

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    if (aFactory == nsnull)
        return NS_ERROR_NULL_POINTER;

    nsIGenericFactory* fact;
    if (aClass.Equals(kFTPDirListingConverterCID)) {
        rv = NS_NewGenericFactory(&fact, nsFTPDirListingConv::Create);
    }
    else if (aClass.Equals(kMultiMixedConverterCID)) {
        rv = NS_NewGenericFactory(&fact, nsMultiMixedConv::Create);
    }
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

    NS_WITH_SERVICE(nsIRegistry, registry, kRegistryCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // open the registry
    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(rv)) return rv;

    // set the key
    nsIRegistry::Key key, key1;

    // root key addition
    rv = registry->AddSubtree(nsIRegistry::Common, NS_ISTREAMCONVERTER_KEY, &key);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;


    // FTP dir listings
    rv = compMgr->RegisterComponent(kFTPDirListingConverterCID,  
                                    "FTPDirListingConverter",
                                    NS_ISTREAMCONVERTER_KEY "?from=text/ftp-dir-unix?to=application/http-index-format",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=text/ftp-dir-unix?to=application/http-index-format", &key1);
    if (NS_FAILED(rv)) return rv;


    rv = compMgr->RegisterComponent(kFTPDirListingConverterCID,  
                                    "FTPDirListingConverter",
                                    NS_ISTREAMCONVERTER_KEY "?from=text/ftp-dir-nt?to=application/http-index-format",
                                    aPath, PR_TRUE, PR_TRUE);

    rv = registry->AddSubtreeRaw(key, "?from=text/ftp-dir-nt?to=application/http-index-format", &key1);
    if (NS_FAILED(rv)) return rv;
    // END FTP dir listings

    // multi-mixed-replace
    rv = compMgr->RegisterComponent(kMultiMixedConverterCID,
                                    "MultiMixedConverter",
                                    NS_ISTREAMCONVERTER_KEY "?from=multipart/x-mixed-replace?to=text/html",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = registry->AddSubtreeRaw(key, "?from=multipart/x-mixed-replace?to=text/html", &key1);
    if (NS_FAILED(rv)) return rv;
    // END mutli-mixed-replace

    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kFTPDirListingConverterCID, aPath);
    if (NS_FAILED(rv)) return rv;
 
    rv = compMgr->UnregisterComponent(kMultiMixedConverterCID, aPath);

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
