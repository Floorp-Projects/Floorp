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
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nscore.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPCID.h"
#include "nsHTTPHandlerFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"
#include "nsIProtocolHandler.h" // for NS_NETWORK_PROTOCOL_PROGID_PREFIX

static NS_DEFINE_CID(kHTTPHandlerCID,      NS_HTTP_HANDLER_FACTORY_CID);

////////////////////////////////////////////////////////////////////////

class nsHTTPHandlerModule : public nsIModule
{
public:
    nsHTTPHandlerModule();
    virtual ~nsHTTPHandlerModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIFactory> mFactory;
};

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsHTTPHandlerModule::nsHTTPHandlerModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsHTTPHandlerModule::~nsHTTPHandlerModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsHTTPHandlerModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsHTTPHandlerModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsHTTPHandlerModule::Shutdown()
{
    // Release the factory object
    mFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsHTTPHandlerModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    nsCOMPtr<nsIFactory> fact;
    if (aClass.Equals(kHTTPHandlerCID)) {
        if (!mFactory) {
            nsHTTPHandlerFactory* factory = new nsHTTPHandlerFactory(aClass);
            if (!factory) {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
            else {
                mFactory = factory;
            }
        }
        fact = mFactory;
    }
    else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsHTTPHandlerModule: unable to create factory for %s\n", cs);
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
    { "HTTP Handler", &kHTTPHandlerCID,
      NS_NETWORK_PROTOCOL_PROGID_PREFIX "http", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsHTTPHandlerModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                  nsIFileSpec* aPath,
                                  const char* registryLocation,
                                  const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering http: components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsHTTPHandlerModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPHandlerModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                    nsIFileSpec* aPath,
                                    const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering http: components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsHTTPHandlerModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandlerModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsHTTPHandlerModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsHTTPHandlerModule: Module already created.");

    // Create and initialize the module instance
    nsHTTPHandlerModule *m = new nsHTTPHandlerModule();
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
