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
#include "prtypes.h"
#include "nspr.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"



#if 1

#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsJVMManager.h"
#include "nsCJVMManagerFactory.h"

static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);




// Module implementation
class nsCJVMManagerModule : public nsIModule
{
public:
    nsCJVMManagerModule();
    virtual ~nsCJVMManagerModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mCJVMManager;
};



//----------------------------------------------------------------------



nsCJVMManagerModule::nsCJVMManagerModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsCJVMManagerModule::~nsCJVMManagerModule()
{
    Shutdown();
}



NS_IMPL_ISUPPORTS(nsCJVMManagerModule, NS_GET_IID(nsIModule))



// Perform our one-time intialization for this module
nsresult
nsCJVMManagerModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}



// Shutdown this module, releasing all of the module resources
void
nsCJVMManagerModule::Shutdown()
{
    // Release the factory object
    mCJVMManager = nsnull;
}



// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsCJVMManagerModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kJVMManagerCID))
    {
    	if (!mCJVMManager) {
            rv = NS_NewGenericFactory(getter_AddRefs(mCJVMManager),
                                      nsJVMManager::Create);
        }
        fact = mCJVMManager;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsCJVMManagerModule: unable to create factory for %s\n", cs);
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
    { "JVM Manager Service", &kJVMManagerCID,  "component://netscape/oji/jvm-mgr" },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))



NS_IMETHODIMP
nsCJVMManagerModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering nsCJVMManagerModule\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsCJVMManagerModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}



NS_IMETHODIMP
nsCJVMManagerModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering nsCJVMManagerModule\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsCJVMManagerModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}



NS_IMETHODIMP
nsCJVMManagerModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}



//----------------------------------------------------------------------

static nsCJVMManagerModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsCJVMManagerModule *m = new nsCJVMManagerModule();
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



#include "nsJVMManager.h"
#include "nsCJVMManagerFactory.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIServiceManagerIID,    NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

///////////////////////////////////////////////////////////////////////////////
// Auto-registration functions START
///////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, 
                                             const char *path)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) {
        return rv;
    }

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = compMgr->RegisterComponent(kJVMManagerCID,
                                    "JVM Manager Service",
                                    "component://netscape/oji/jvm-mgr",
                                    path, 
                                    PR_TRUE, PR_TRUE);

    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, 
                                               const char *path)
{
    nsresult rv;
    
    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) {
        return rv;
    }

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    rv = compMgr->UnregisterComponent(kJVMManagerCID, path);

    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NSGetFactory:
 * Provides entry point to liveconnect dll.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (!aClass.Equals(kJVMManagerCID)) {
        return NS_ERROR_FACTORY_NOT_LOADED;     // XXX right error?
    }

    nsCJVMManagerFactory* factory = new nsCJVMManagerFactory();
    if (factory == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    NS_ADDREF(factory);
    *aFactory = factory;
    
    return NS_OK;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return PR_FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Auto-registration functions END
///////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
// from nsISupports 

#if 1
NS_METHOD
nsCJVMManagerFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    PR_ASSERT(NULL != aInstancePtr); 
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(kIFactoryIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    }
    return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsCJVMManagerFactory)
NS_IMPL_RELEASE(nsCJVMManagerFactory)
#else
NS_IMPL_ISUPPORTS(nsCJVMManagerFactory, kIFactoryIID)
#endif

////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsCJVMManagerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
	return nsJVMManager::Create(aOuter, aIID, aResult);
}

NS_METHOD
nsCJVMManagerFactory::LockFactory(PRBool aLock)
{
   return NS_OK;
}



////////////////////////////////////////////////////////////////////////////
// from nsCJVMManagerFactory:

nsCJVMManagerFactory::nsCJVMManagerFactory(void)
{
      NS_INIT_REFCNT();
}

nsCJVMManagerFactory::~nsCJVMManagerFactory()
{
}


#endif
