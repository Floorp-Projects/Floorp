/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#if 1

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIMIMEService.h"
#include "nsMIMEService.h"
#include "nsMIMEServiceFactory.h"
#include "nsIProtocolHandler.h" // for NS_NETWORK_PROTOCOL_PROGID_PREFIX

static NS_DEFINE_CID(kMIMEServiceCID,      NS_MIMESERVICE_CID);



//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP
CreateNewMIMEService(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{
    if (!aResult) {
        return NS_ERROR_INVALID_POINTER;
    }
    if (aOuter) {
        *aResult = nsnull;
        return NS_ERROR_NO_AGGREGATION;
    }
    nsMIMEService *MIMEService = new nsMIMEService();
    if (!MIMEService)
    	return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(MIMEService);
    nsresult rv = MIMEService->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
        *aResult = nsnull;
    }
    NS_RELEASE(MIMEService);
    return rv;
}



// Module implementation
class nsMIMEServiceModule : public nsIModule
{
public:
    nsMIMEServiceModule();
    virtual ~nsMIMEServiceModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mMIMEServiceModuleFactory;
};



//----------------------------------------------------------------------



nsMIMEServiceModule::nsMIMEServiceModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsMIMEServiceModule::~nsMIMEServiceModule()
{
    Shutdown();
}



NS_IMPL_ISUPPORTS(nsMIMEServiceModule, NS_GET_IID(nsIModule))



// Perform our one-time intialization for this module
nsresult
nsMIMEServiceModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsMIMEServiceModule::Shutdown()
{
    // Release the factory object
    mMIMEServiceModuleFactory = nsnull;
}



// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsMIMEServiceModule::GetClassObject(nsIComponentManager *aCompMgr,
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

    nsCOMPtr<nsIGenericFactory> fact;

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    if (aClass.Equals(kMIMEServiceCID))
    {
    	if (!mMIMEServiceModuleFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mMIMEServiceModuleFactory),
                                      CreateNewMIMEService);
        }
        fact = mMIMEServiceModuleFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsMIMEServiceModule: unable to create factory for %s\n", cs);
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
    { "MIME type file extensions mapping", &kMIMEServiceCID, "component:||netscape|mime" },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))



NS_IMETHODIMP
nsMIMEServiceModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering nsMIMEServiceModule\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsMIMEServiceModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}



NS_IMETHODIMP
nsMIMEServiceModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering nsMIMEServiceModule\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsMIMEServiceModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}



NS_IMETHODIMP
nsMIMEServiceModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}



//----------------------------------------------------------------------

static nsMIMEServiceModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsMIMEServiceModule *m = new nsMIMEServiceModule();
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



#else


/*

    The nsHTTPHandlerFactory implementation. This was directly
    plagiarized from Chris Waterson the Great. So if you find 
    a fault here... make sure you notify him as well. 

    -Gagan Saksena 03/25/99

*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIMIMEService.h"
#include "nsMIMEService.h"
#include "nsMIMEServiceFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"
#include "nsIProtocolHandler.h" // for NS_NETWORK_PROTOCOL_PROGID_PREFIX

static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kMIMEServiceCID,      NS_MIMESERVICE_CID);

////////////////////////////////////////////////////////////////////////

nsMIMEServiceFactory::nsMIMEServiceFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_REFCNT();
}

nsMIMEServiceFactory::~nsMIMEServiceFactory()
{
}

NS_IMETHODIMP
nsMIMEServiceFactory::QueryInterface(const nsIID &aIID, void **aResult)
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

NS_IMPL_ADDREF(nsMIMEServiceFactory);
NS_IMPL_RELEASE(nsMIMEServiceFactory);

NS_IMETHODIMP
nsMIMEServiceFactory::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv = NS_OK;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kMIMEServiceCID)) {
        nsMIMEService *MIMEService = new nsMIMEService();
        if (!MIMEService) return NS_ERROR_OUT_OF_MEMORY;
        MIMEService->QueryInterface(NS_GET_IID(nsISupports), (void**)&inst);
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(inst);
    *aResult = inst;
    NS_RELEASE(inst);
    return rv;
}

nsresult nsMIMEServiceFactory::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////



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

    nsMIMEServiceFactory* factory = new nsMIMEServiceFactory(aClass, aClassName, aProgID);
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
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kMIMEServiceCID,
                                    "MIME type file extensions mapping",
                                    "component:||netscape|mime",
                                    aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kMIMEServiceCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

#endif
