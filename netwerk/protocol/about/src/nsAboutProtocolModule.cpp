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
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsAboutProtocolHandler.h"
#include "nsAboutBlank.h"
#include "nsAboutBloat.h"
#include "nscore.h"

static NS_DEFINE_CID(kAboutProtocolHandlerCID,  NS_ABOUTPROTOCOLHANDLER_CID);
static NS_DEFINE_CID(kAboutBlankModuleCID,      NS_ABOUT_BLANK_MODULE_CID);
static NS_DEFINE_CID(kAboutBloatModuleCID,      NS_ABOUT_BLOAT_MODULE_CID);

///////////////////////////////////////////////////////////////////////////////

class nsAboutProtocolModule : public nsIModule
{
public:
    nsAboutProtocolModule();
    virtual ~nsAboutProtocolModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mAboutProtocolFactory;
    nsCOMPtr<nsIGenericFactory> mAboutBlankFactory;
    nsCOMPtr<nsIGenericFactory> mAboutBloatFactory;
};

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsAboutProtocolModule::nsAboutProtocolModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsAboutProtocolModule::~nsAboutProtocolModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsAboutProtocolModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsAboutProtocolModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsAboutProtocolModule::Shutdown()
{
    // Release the factory objects
    mAboutProtocolFactory = nsnull;
    mAboutBlankFactory = nsnull;
    mAboutBloatFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsAboutProtocolModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kAboutProtocolHandlerCID)) {
        if (!mAboutProtocolFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mAboutProtocolFactory),
                                      nsAboutProtocolHandler::Create);
        }
        fact = mAboutProtocolFactory;
    }
    else if (aClass.Equals(kAboutBlankModuleCID)) {
        if (!mAboutBlankFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mAboutBlankFactory),
                                      nsAboutBlank::Create);
        }
        fact = mAboutBlankFactory;
    }
    else if (aClass.Equals(kAboutBloatModuleCID)) {
        if (!mAboutBloatFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mAboutBloatFactory),
                                      nsAboutBloat::Create);
        }
        fact = mAboutBloatFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsAboutProtocolModule: unable to create factory for %s\n", cs);
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
    { "About Protocol Handler", &kAboutProtocolHandlerCID,
      NS_NETWORK_PROTOCOL_PROGID_PREFIX "about", },
    { "about:blank", &kAboutBlankModuleCID,
      NS_ABOUT_MODULE_PROGID_PREFIX "blank", },
    { "about:bloat", &kAboutBloatModuleCID,
      NS_ABOUT_MODULE_PROGID_PREFIX "bloat", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsAboutProtocolModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                    nsIFileSpec* aPath,
                                    const char* registryLocation,
                                    const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering about: components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsAboutProtocolModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsAboutProtocolModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                      nsIFileSpec* aPath,
                                      const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering about: components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsAboutProtocolModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsAboutProtocolModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsAboutProtocolModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsAboutProtocolModule: Module already created.");

    // Create and initialize the module instance
    nsAboutProtocolModule *m = new nsAboutProtocolModule();
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
