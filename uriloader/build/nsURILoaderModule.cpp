/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsURILoader.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kURILoaderCID,          NS_URI_LOADER_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsURILoader)


// Module implementation for the sample library
class nsURILoaderModule : public nsIModule
{
public:
    nsURILoaderModule();
    virtual ~nsURILoaderModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;

    nsCOMPtr<nsIGenericFactory> mURILoaderFactory;
};

nsURILoaderModule::nsURILoaderModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsURILoaderModule::~nsURILoaderModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsURILoaderModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult nsURILoaderModule::Initialize()
{
    if (mInitialized)
        return NS_OK;

    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void nsURILoaderModule::Shutdown()
{
    // Release the factory object
    mURILoaderFactory = null_nsCOMPtr();

}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP nsURILoaderModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj)
        return NS_ERROR_INVALID_POINTER;

    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) 
    {
        rv = Initialize();
        if (NS_FAILED(rv)) // Initialization failed! yikes!
            return rv;
    }
    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;

    if (aClass.Equals(kURILoaderCID))
    {
        if (!mURILoaderFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mURILoaderFactory), &nsURILoaderConstructor);
        fact = mURILoaderFactory;
    } 
    
    if (fact)
        rv = fact->QueryInterface(aIID, r_classObj);

    return rv;
}

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    { "Netscape URI Loader Service", &kURILoaderCID,
      NS_URI_LOADER_PROGID }
};

#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP nsURILoaderModule::RegisterSelf(nsIComponentManager *aCompMgr,
                          nsIFileSpec* aPath,
                          const char* registryLocation,
                          const char* componentType)
{
    nsresult rv = NS_OK;

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) 
            break;
        cp++;
    }
	  
    return rv;
}

NS_IMETHODIMP nsURILoaderModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                            nsIFileSpec* aPath,
                            const char* registryLocation)
{
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
        aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP nsURILoaderModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload)
        return NS_ERROR_INVALID_POINTER;

    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsURILoaderModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsURILoaderModule: Module already created.");

    // Create an initialize the imap module instance
    nsURILoaderModule *module = new nsURILoaderModule();
    if (!module)
        return NS_ERROR_OUT_OF_MEMORY;

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = module->QueryInterface(nsIModule::GetIID(), (void**)return_cobj);
    if (NS_FAILED(rv)) 
    {
        delete module;
        module = nsnull;
    }
    gModule = module;                  // WARNING: Weak Reference
    return rv;
}
