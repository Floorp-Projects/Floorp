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
#include "nsDataHandler.h"
#include "nscore.h"

static NS_DEFINE_CID(kComponentManagerCID,      NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kDataHandlerCID,   NS_DATAHANDLER_CID);

////////////////////////////////////////////////////////////////////////////////

// return the proper factory to the caller
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
    if (aClass.Equals(kDataHandlerCID)) {
        rv = NS_NewGenericFactory(&fact, nsDataHandler::Create);
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

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kDataHandlerCID,  
                                    "Data Protocol Handler",
                                    NS_NETWORK_PROTOCOL_PROGID_PREFIX "data",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;;

    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kDataHandlerCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
