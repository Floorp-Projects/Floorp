/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8; c-file-style: "stroustrup" -*-
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
 * Original Author(s):
 *   Robert John Churchill           <rjc@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf                  <pp@ludusdesign.com>
 */

#include "nsRDFCID.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIRDFDataSource.h"
#include "nsISearchService.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kLocalSearchServiceCID,         NS_RDFFINDDATASOURCE_CID);
static NS_DEFINE_CID(kInternetSearchServiceCID,      NS_RDFSEARCHDATASOURCE_CID);
static NS_DEFINE_CID(kComponentManagerCID,           NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID,             NS_GENERICFACTORY_CID);

NS_IMETHODIMP	NS_NewLocalSearchService(nsISupports* aOuter, REFNSIID aIID, void** aResult);
NS_IMETHODIMP	NS_NewInternetSearchService(nsISupports* aOuter, REFNSIID aIID, void** aResult);




#if 1



// Module implementation
class nsSearchModule : public nsIModule
{
public:
    nsSearchModule();
    virtual ~nsSearchModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mInternetSearchFactory;
    nsCOMPtr<nsIGenericFactory> mLocalSearchFactory;
};



nsSearchModule::nsSearchModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsSearchModule::~nsSearchModule()
{
    Shutdown();
}



// Perform our one-time intialization for this module
nsresult
nsSearchModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}



// Shutdown this module, releasing all of the module resources
void
nsSearchModule::Shutdown()
{
    // Release the factory object
    mInternetSearchFactory = nsnull;
    mLocalSearchFactory = nsnull;
}



NS_IMPL_ISUPPORTS(nsSearchModule, NS_GET_IID(nsIModule))






// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsSearchModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kInternetSearchServiceCID))
    {
    	if (!mInternetSearchFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mInternetSearchFactory),
                                      NS_NewInternetSearchService);
        }
        fact = mInternetSearchFactory;
    }
    else if (aClass.Equals(kLocalSearchServiceCID))
    {
    	if (!mLocalSearchFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mLocalSearchFactory),
                                      NS_NewLocalSearchService);
        }
        fact = mLocalSearchFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsSearchModule: unable to create factory for %s\n", cs);
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
    { "LocalSearch", &kLocalSearchServiceCID, NS_LOCALSEARCH_SERVICE_PROGID },
    { "LocalSearch", &kLocalSearchServiceCID, NS_LOCALSEARCH_DATASOURCE_PROGID },
    { "InternetSearch", &kInternetSearchServiceCID, NS_INTERNETSEARCH_SERVICE_PROGID },
    { "InternetSearch", &kInternetSearchServiceCID, NS_INTERNETSEARCH_DATASOURCE_PROGID },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))


NS_IMETHODIMP
nsSearchModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFile* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering nsSearchModule\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsSearchModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}



NS_IMETHODIMP
nsSearchModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFile* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering nsSearchModule\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsSearchModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}



NS_IMETHODIMP
nsSearchModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}



//----------------------------------------------------------------------

static nsSearchModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* aPath,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsSearchModule *m = new nsSearchModule();
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



////////////////////////////////////////////////////////////////////////
// Component Exports



extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
	NS_PRECONDITION(aFactory != nsnull, "null ptr");
	if (! aFactory)
		return NS_ERROR_NULL_POINTER;

	nsIGenericFactory::ConstructorProcPtr constructor;

	if (aClass.Equals(kInternetSearchServiceCID)) {
		constructor = NS_NewInternetSearchService;
	}
	else if (aClass.Equals(kLocalSearchServiceCID)) {
		constructor = NS_NewLocalSearchService;
	}
	else {
		*aFactory = nsnull;
		return NS_NOINTERFACE; // XXX
	}

	nsresult rv;
	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIGenericFactory> factory;
	rv = compMgr->CreateInstance(kGenericFactoryCID,
				     nsnull,
				     NS_GET_IID(nsIGenericFactory),
				     getter_AddRefs(factory));

	if (NS_FAILED(rv)) return rv;

	rv = factory->SetConstructor(constructor);
	if (NS_FAILED(rv)) return rv;

	*aFactory = factory;
	NS_ADDREF(*aFactory);
	return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kLocalSearchServiceCID, "LocalSearch",
					NS_LOCALSEARCH_SERVICE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kLocalSearchServiceCID, "LocalSearch",
					NS_LOCALSEARCH_DATASOURCE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kInternetSearchServiceCID, "InternetSearch",
					NS_INTERNETSEARCH_SERVICE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	rv = compMgr->RegisterComponent(kInternetSearchServiceCID, "InternetSearch",
					NS_INTERNETSEARCH_DATASOURCE_PROGID,
					aPath, PR_TRUE, PR_TRUE);

	return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kLocalSearchServiceCID, aPath);
	rv = compMgr->UnregisterComponent(kInternetSearchServiceCID, aPath);

	return NS_OK;
}

#endif
