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
#include "nsIOService.h"
#include "nsNetModuleMgr.h"
//#include "nsFileTransportService.h"
#include "nsSocketTransportService.h"
#include "nsSocketProviderService.h"
#include "nscore.h"
#include "nsStdURL.h"
#include "nsSimpleURI.h"
#include "nsDnsService.h"
#include "nsLoadGroup.h"
#include "nsInputStreamChannel.h"

static NS_DEFINE_CID(kComponentManagerCID,       NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
//static NS_DEFINE_CID(kFileTransportServiceCID,   NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kSimpleURICID,              NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kSocketProviderServiceCID,  NS_SOCKETPROVIDERSERVICE_CID);
static NS_DEFINE_CID(kExternalModuleManagerCID,  NS_NETMODULEMGR_CID);
static NS_DEFINE_CID(kDNSServiceCID,             NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kLoadGroupCID,              NS_LOADGROUP_CID);
static NS_DEFINE_CID(kInputStreamChannelCID,     NS_INPUTSTREAMCHANNEL_CID);

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
    if (aClass.Equals(kIOServiceCID)) {
        rv = NS_NewGenericFactory(&fact, nsIOService::Create);
    }
#if 0
    else if (aClass.Equals(kFileTransportServiceCID)) {
        rv = NS_NewGenericFactory(&fact, nsFileTransportService::Create);
    }
#endif
    else if (aClass.Equals(kSocketTransportServiceCID)) {
        rv = NS_NewGenericFactory(&fact, nsSocketTransportService::Create);
    }
    else if (aClass.Equals(kSocketProviderServiceCID)) {
        rv = NS_NewGenericFactory(&fact, nsSocketProviderService::Create);
    }
    else if (aClass.Equals(kDNSServiceCID)) {
        rv = NS_NewGenericFactory(&fact, nsDNSService::Create);
    }
    else if (aClass.Equals(kStandardURLCID)) {
        rv = NS_NewGenericFactory(&fact, nsStdURL::Create);
    }
    else if (aClass.Equals(kSimpleURICID)) {
        rv = NS_NewGenericFactory(&fact, nsSimpleURI::Create);
    }
    else if (aClass.Equals(kExternalModuleManagerCID)) {
        rv = NS_NewGenericFactory(&fact, nsNetModuleMgr::Create);
    }
    else if (aClass.Equals(kDNSServiceCID)) {
        rv = NS_NewGenericFactory(&fact, nsDNSService::Create);
    }
    else if (aClass.Equals(kLoadGroupCID)) {
        rv = NS_NewGenericFactory(&fact, nsLoadGroup::Create);
    }
    else if (aClass.Equals(kInputStreamChannelCID)) {
        rv = NS_NewGenericFactory(&fact, nsInputStreamChannel::Create);
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

    rv = compMgr->RegisterComponent(kIOServiceCID,  
                                    "Network Service",
                                    "component://netscape/network/net-service",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
#if 0
    rv = compMgr->RegisterComponent(kFileTransportServiceCID, 
                                    "File Transport Service",
                                    "component://netscape/network/file-transport-service",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;
#endif
    rv = compMgr->RegisterComponent(kSocketTransportServiceCID, 
                                    "Socket Transport Service",
                                    "component://netscape/network/socket-transport-service",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kSocketProviderServiceCID, 
                                    "Socket Provider Service",
                                    "component://netscape/network/socket-provider-service",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kDNSServiceCID, 
                                    "DNS Service",
                                    "component://netscape/network/dns-service",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kStandardURLCID, 
                                    "Standard URL Implementation",
                                    "component://netscape/network/standard-url",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kSimpleURICID, 
                                    "Simple URI Implementation",
                                    "component://netscape/network/simple-uri",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kExternalModuleManagerCID,
                                    "External Module Manager",
                                    "component://netscape/network/net-extern-mod",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kInputStreamChannelCID,
                                    "Input Stream Channel",
                                    "component://netscape/network/input-stream-channel",
                                    aPath, PR_TRUE, PR_TRUE);
    return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kIOServiceCID, aPath);
    if (NS_FAILED(rv)) return rv;
#if 0
    rv = compMgr->UnregisterComponent(kFileTransportServiceCID, aPath);
    if (NS_FAILED(rv)) return rv;
#endif
    rv = compMgr->UnregisterComponent(kSocketTransportServiceCID, aPath);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kSocketProviderServiceCID, aPath);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kDNSServiceCID, aPath);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kStandardURLCID, aPath);
    if (NS_FAILED(rv)) return rv;
    
    rv = compMgr->UnregisterComponent(kSimpleURICID, aPath);
    if (NS_FAILED(rv)) return rv;
    
    rv = compMgr->UnregisterComponent(kExternalModuleManagerCID, aPath);
    if (NS_FAILED(rv)) return rv;
    
    rv = compMgr->UnregisterComponent(kInputStreamChannelCID, aPath);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
