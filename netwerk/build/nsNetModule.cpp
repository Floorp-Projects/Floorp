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
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIOService.h"
#include "nsNetModuleMgr.h"
#include "nsFileTransportService.h"
#include "nsSocketTransportService.h"
#include "nsSocketProviderService.h"
#include "nscore.h"
#include "nsStdURL.h"
#include "nsSimpleURI.h"
#include "nsDnsService.h"
#include "nsLoadGroup.h"
#include "nsInputStreamChannel.h"
#include "nsUnicharStreamLoader.h"

static NS_DEFINE_CID(kComponentManagerCID,       NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kIOServiceCID,              NS_IOSERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID,   NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID,            NS_STANDARDURL_CID);
static NS_DEFINE_CID(kSimpleURICID,              NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kSocketProviderServiceCID,  NS_SOCKETPROVIDERSERVICE_CID);
static NS_DEFINE_CID(kExternalModuleManagerCID,  NS_NETMODULEMGR_CID);
static NS_DEFINE_CID(kDNSServiceCID,             NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kLoadGroupCID,              NS_LOADGROUP_CID);
static NS_DEFINE_CID(kInputStreamChannelCID,     NS_INPUTSTREAMCHANNEL_CID);
static NS_DEFINE_CID(kUnicharStreamLoaderCID,    NS_UNICHARSTREAMLOADER_CID);

///////////////////////////////////////////////////////////////////////////////

// Module implementation for the net library
class nsNetModule : public nsIModule
{
public:
    nsNetModule();
    virtual ~nsNetModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mIOServiceFactory;
    nsCOMPtr<nsIGenericFactory> mFileTransportServiceFactory;
    nsCOMPtr<nsIGenericFactory> mSocketTransportServiceFactory;
    nsCOMPtr<nsIGenericFactory> mSocketProviderServiceFactory;
    nsCOMPtr<nsIGenericFactory> mDNSServiceFactory;
    nsCOMPtr<nsIGenericFactory> mStandardURLFactory;
    nsCOMPtr<nsIGenericFactory> mSimpleURIFactory;
    nsCOMPtr<nsIGenericFactory> mExternalModuleManagerFactory;
    nsCOMPtr<nsIGenericFactory> mLoadGroupFactory;
    nsCOMPtr<nsIGenericFactory> mInputStreamChannelFactory;
    nsCOMPtr<nsIGenericFactory> mUnicharStreamLoaderFactory;
};

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsNetModule::nsNetModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsNetModule::~nsNetModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsNetModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsNetModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsNetModule::Shutdown()
{
    // Release the factory objects
    mIOServiceFactory = nsnull;
    mFileTransportServiceFactory = nsnull;
    mSocketTransportServiceFactory = nsnull;
    mSocketProviderServiceFactory = nsnull;
    mDNSServiceFactory = nsnull;
    mStandardURLFactory = nsnull;
    mSimpleURIFactory = nsnull;
    mExternalModuleManagerFactory = nsnull;
    mLoadGroupFactory = nsnull;
    mInputStreamChannelFactory = nsnull;
    mUnicharStreamLoaderFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsNetModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            // Initialization failed! yikes!
            return rv;
        }
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;
    if (aClass.Equals(kIOServiceCID)) {
        if (!mIOServiceFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mIOServiceFactory),
                                      nsIOService::Create);
        }
        fact = mIOServiceFactory;
    }
    else if (aClass.Equals(kFileTransportServiceCID)) {
        if (!mFileTransportServiceFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mFileTransportServiceFactory),
                                      nsFileTransportService::Create);
        }
        fact = mFileTransportServiceFactory;
    }
    else if (aClass.Equals(kSocketTransportServiceCID)) {
        if (!mSocketTransportServiceFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mSocketTransportServiceFactory),
                                      nsSocketTransportService::Create);
        }
        fact = mSocketTransportServiceFactory;
    }
    else if (aClass.Equals(kSocketProviderServiceCID)) {
        if (!mSocketProviderServiceFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mSocketProviderServiceFactory),
                                      nsSocketProviderService::Create);
        }
        fact = mSocketProviderServiceFactory;
    }
    else if (aClass.Equals(kDNSServiceCID)) {
        if (!mDNSServiceFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mDNSServiceFactory),
                                      nsDNSService::Create);
        }
        fact = mDNSServiceFactory;
    }
    else if (aClass.Equals(kStandardURLCID)) {
        if (!mStandardURLFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mStandardURLFactory),
                                      nsStdURL::Create);
        }
        fact = mStandardURLFactory;
    }
    else if (aClass.Equals(kSimpleURICID)) {
        if (!mSimpleURIFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mSimpleURIFactory),
                                      nsSimpleURI::Create);
        }
        fact = mSimpleURIFactory;
    }
    else if (aClass.Equals(kExternalModuleManagerCID)) {
        if (!mExternalModuleManagerFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mExternalModuleManagerFactory),
                                      nsNetModuleMgr::Create);
        }
        fact = mExternalModuleManagerFactory;
    }
    else if (aClass.Equals(kLoadGroupCID)) {
        if (!mLoadGroupFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mLoadGroupFactory),
                                      nsLoadGroup::Create);
        }
        fact = mLoadGroupFactory;
    }
    else if (aClass.Equals(kInputStreamChannelCID)) {
        if (!mInputStreamChannelFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mInputStreamChannelFactory),
                                      nsInputStreamChannel::Create);
        }
        fact = mInputStreamChannelFactory;
    }
    else if (aClass.Equals(kUnicharStreamLoaderCID)) {
        if (!mUnicharStreamLoaderFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mUnicharStreamLoaderFactory),
                                      nsUnicharStreamLoader::Create);
        }
        fact = mUnicharStreamLoaderFactory;
    }
    else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsNetModule: unable to create factory for %s\n", cs);
        nsCRT::free(cs);
#endif
    }

    if (fact) {
        rv = fact->QueryInterface(aIID, r_classObj);
    }

    return rv;
}

//----------------------------------------

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    { "Network Service", &kIOServiceCID,
      "component://netscape/network/net-service", },
    { "File Transport Service", &kFileTransportServiceCID,
      "component://netscape/network/file-transport-service", },
    { "Socket Transport Service", &kSocketTransportServiceCID,
      "component://netscape/network/socket-transport-service", },
    { "Socket Provider Service", &kSocketProviderServiceCID,
      "component://netscape/network/socket-provider-service", },
    { "DNS Service", &kDNSServiceCID,
      "component://netscape/network/dns-service", },
    { "Standard URL Implementation", &kStandardURLCID,
      "component://netscape/network/standard-url", },
    { "Simple URI Implementation", &kSimpleURICID,
      "component://netscape/network/simple-uri", },
    { "External Module Manager", &kExternalModuleManagerCID,
      "component://netscape/network/net-extern-mod", },
    { "Input Stream Channel", &kInputStreamChannelCID,
      "component://netscape/network/input-stream-channel", },
    { "Unichar Stream Loader", &kUnicharStreamLoaderCID,
      "component://netscape/network/unichar-stream-loader", },
    { "Load Group", &kLoadGroupCID,
      "component://netscape/network/load-group", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsNetModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec* aPath,
                          const char* registryLocation,
                          const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering net components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsNetModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsNetModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFileSpec* aPath,
                            const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering net components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsNetModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsNetModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsNetModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsNetModule: Module already created.");

    // Create and initialize the module instance
    nsNetModule *m = new nsNetModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}
