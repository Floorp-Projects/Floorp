/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "OJITestLoader.h"
#include "OJITestLoaderFactory.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIServiceManagerIID,    NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kOJITestLoaderCID, OJITESTLOADER_CID);

static const char* loaderRegistryPath = "@mozilla.org/oji/test/api/loader;1";
static const char* loaderRegistryDesc = "OJI API Test Loader";

// The list of components we register
static const nsModuleComponentInfo components[] = 
{
    { loaderRegistryDesc, 
      OJITESTLOADER_CID,  
      loaderRegistryPath, 
      OJITestLoader::Create
    },
};

NS_IMPL_NSGETMODULE(OJITestLoader, components);

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, 
                                             const char *path)
{
    printf("Registering OJITestLoader\n");

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

    rv = compMgr->RegisterComponent(kOJITestLoaderCID,
                                    loaderRegistryDesc,
                                    loaderRegistryPath,
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
    
    rv = compMgr->UnregisterComponent(kOJITestLoaderCID, path);

    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aContractID,
             nsIFactory **aFactory)
{
    if (!aClass.Equals(kOJITestLoaderCID)) {
        return NS_ERROR_FACTORY_NOT_LOADED;     // XXX right error?
    }

    OJITestLoaderFactory* factory = new OJITestLoaderFactory();
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

NS_METHOD
OJITestLoaderFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
  //PR_ASSERT(NULL != aInstancePtr); 
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

NS_IMPL_ADDREF(OJITestLoaderFactory)
NS_IMPL_RELEASE(OJITestLoaderFactory)

NS_METHOD
OJITestLoaderFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    OJITestLoader* loader = new OJITestLoader();
    if (loader == NULL) 
        return NS_ERROR_OUT_OF_MEMORY;
    loader->AddRef();
    *aResult = loader;
    return NS_OK;
}


NS_METHOD
OJITestLoaderFactory::LockFactory(PRBool aLock)
{
   return NS_OK;
}

OJITestLoaderFactory::OJITestLoaderFactory(void)
{
}

OJITestLoaderFactory::~OJITestLoaderFactory()
{
}
