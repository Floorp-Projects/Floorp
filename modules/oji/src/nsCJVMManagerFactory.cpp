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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#if 1

#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsJVMManager.h"

// The list of components we register
static nsModuleComponentInfo components[] = 
{
    { "JVM Manager Service", 
      NS_JVMMANAGER_CID,  
      "@mozilla.org/oji/jvm-mgr;1", 
      nsJVMManager::Create
    },
};

NS_IMPL_NSGETMODULE("nsCJVMManagerModule", components);

#else

#include "prtypes.h"
#include "nspr.h"
#include "prmem.h"
#include "prmon.h"
#include "prlog.h"




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
                             NS_GET_IID(nsIComponentManager), 
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) {
        return rv;
    }

    rv = compMgr->RegisterComponent(kJVMManagerCID,
                                    "JVM Manager Service",
                                    "@mozilla.org/oji/jvm-mgr;1",
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
                             NS_GET_ID(nsIComponentManager), 
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
             const char *aContractID,
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
