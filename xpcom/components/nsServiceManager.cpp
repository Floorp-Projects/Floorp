/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIServiceManager.h"
#include "nsVector.h"
#include "nsHashtable.h"
#include "prcmon.h"
#include "prthread.h" /* XXX: only used for the NSPR initialization hack (rick) */

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

class nsServiceEntry {
public:

    nsServiceEntry(const nsCID& cid, nsISupports* service);
    ~nsServiceEntry();

    nsresult AddListener(nsIShutdownListener* listener);
    nsresult RemoveListener(nsIShutdownListener* listener);
    nsresult NotifyListeners(void);

    const nsCID& mClassID;
    nsISupports* mService;
    nsVector* mListeners;        // nsVector<nsIShutdownListener>
    PRBool mShuttingDown;

};

nsServiceEntry::nsServiceEntry(const nsCID& cid, nsISupports* service)
    : mClassID(cid), mService(service), mListeners(NULL), mShuttingDown(PR_FALSE)
{
}

nsServiceEntry::~nsServiceEntry()
{
    if (mListeners) {
        NS_ASSERTION(mListeners->GetSize() == 0, "listeners not removed or notified");
#if 0
        PRUint32 size = mListeners->GetSize();
        for (PRUint32 i = 0; i < size; i++) {
            nsIShutdownListener* listener = (nsIShutdownListener*)(*mListeners)[i];
            NS_RELEASE(listener);
        }
#endif
        delete mListeners;
    }
}

nsresult
nsServiceEntry::AddListener(nsIShutdownListener* listener)
{
    if (listener == NULL)
        return NS_OK;
    if (mListeners == NULL) {
        mListeners = new nsVector();
        if (mListeners == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    PRInt32 rv = mListeners->Add(listener);
    NS_ADDREF(listener);
    return rv == -1 ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsServiceEntry::RemoveListener(nsIShutdownListener* listener)
{
    if (listener == NULL)
        return NS_OK;
    NS_ASSERTION(mListeners, "no listeners added yet");
    PRUint32 size = mListeners->GetSize();
    for (PRUint32 i = 0; i < size; i++) {
        if ((*mListeners)[i] == listener) {
            mListeners->Remove(i);
            NS_RELEASE(listener);
            return NS_OK;
        }
    }
    NS_ASSERTION(0, "unregistered shutdown listener");
    return NS_ERROR_FAILURE;
}

nsresult
nsServiceEntry::NotifyListeners(void)
{
    if (mListeners) {
        PRUint32 size = mListeners->GetSize();
        for (PRUint32 i = 0; i < size; i++) {
            nsIShutdownListener* listener = (nsIShutdownListener*)(*mListeners)[0];
            nsresult rv = listener->OnShutdown(mClassID, mService);
            if (NS_FAILED(rv)) return rv;
            NS_RELEASE(listener);
            mListeners->Remove(0);
        }
        NS_ASSERTION(mListeners->GetSize() == 0, "failed to notify all listeners");
        delete mListeners;
        mListeners = NULL;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsServiceManagerImpl : public nsIServiceManager {
public:

    NS_IMETHOD
    RegisterService(const nsCID& aClass, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const nsCID& aClass);

    NS_IMETHOD
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    RegisterService(const char* aProgID, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const char* aProgID);

    NS_IMETHOD
    GetService(const char* aProgID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const char* aProgID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    nsServiceManagerImpl(void);

    NS_DECL_ISUPPORTS

protected:

    virtual ~nsServiceManagerImpl(void);

    nsHashtable/*<nsServiceEntry>*/* mServices;
};

nsServiceManagerImpl::nsServiceManagerImpl(void)
{
    NS_INIT_REFCNT();
    mServices = new nsHashtable();
    NS_ASSERTION(mServices, "out of memory already?");
}

static PRBool
DeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
    nsServiceEntry* entry = (nsServiceEntry*)aData;
    NS_RELEASE(entry->mService);
    delete entry;
    return PR_TRUE;
}

nsServiceManagerImpl::~nsServiceManagerImpl(void)
{
    if (mServices) {
        mServices->Enumerate(DeleteEntry);
        delete mServices;
    }
}

static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsServiceManagerImpl);
NS_IMPL_RELEASE(nsServiceManagerImpl);

NS_IMETHODIMP
nsServiceManagerImpl::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    *aInstancePtr = NULL; 
    if (aIID.Equals(kIServiceManagerIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
}

NS_IMETHODIMP
nsServiceManagerImpl::GetService(const nsCID& aClass, const nsIID& aIID,
                                 nsISupports* *result,
                                 nsIShutdownListener* shutdownListener)
{
    nsresult rv = NS_OK;
    /* XXX: This is a hack to force NSPR initialization..  This should be 
     *      removed once PR_CEnterMonitor(...) initializes NSPR... (rick)
     */
    (void)PR_GetCurrentThread();
    PR_CEnterMonitor(this);

    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    if (entry) {
        nsISupports* service;
        rv = entry->mService->QueryInterface(aIID, (void**)&service);
        if (NS_SUCCEEDED(rv)) {
            rv = entry->AddListener(shutdownListener);
            if (NS_SUCCEEDED(rv)) {
                *result = service;

                // If someone else requested the service to be shut down, 
                // and we just asked to get it again before it could be 
                // released, then cancel their shutdown request:
                if (entry->mShuttingDown) {
                    entry->mShuttingDown = PR_FALSE;
                    NS_ADDREF(service);      // Released in UnregisterService
                }
            }
        }
    }
    else {
        nsISupports* service;
        rv = nsComponentManager::CreateInstance(aClass, NULL, aIID, (void**)&service);
        if (NS_SUCCEEDED(rv)) {
            entry = new nsServiceEntry(aClass, service);
            if (entry == NULL) {
                NS_RELEASE(service);
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
            else {
                rv = entry->AddListener(shutdownListener);
                if (NS_SUCCEEDED(rv)) {
                    mServices->Put(&key, entry);
                    *result = service;
                    NS_ADDREF(service);      // Released in UnregisterService
                }
                else {
                    NS_RELEASE(service);
                    delete entry;
                }
            }
        }
    }

    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsServiceManagerImpl::ReleaseService(const nsCID& aClass, nsISupports* service,
                                     nsIShutdownListener* shutdownListener)
{
    nsresult rv = NS_OK;
    PR_CEnterMonitor(this);

    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    NS_ASSERTION(entry, "service not found");
    NS_ASSERTION(entry->mService == service, "service looked failed");

    if (entry) {
        rv = entry->RemoveListener(shutdownListener);
        nsrefcnt cnt;
        NS_RELEASE2(service, cnt);
        if (NS_SUCCEEDED(rv) && cnt == 0) {
            mServices->Remove(&key);
            delete entry;
            rv = nsComponentManager::FreeLibraries();
        }
    }

    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsServiceManagerImpl::RegisterService(const nsCID& aClass, nsISupports* aService)
{
    nsresult rv = NS_OK;
    PR_CEnterMonitor(this);

    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);
    if (entry) {
        rv = NS_ERROR_FAILURE;
    }
    else {
        nsServiceEntry* entry = new nsServiceEntry(aClass, aService);
        if (entry == NULL) 
            rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            mServices->Put(&key, entry);
            NS_ADDREF(aService);      // Released in UnregisterService
        }
    }
    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsServiceManagerImpl::UnregisterService(const nsCID& aClass)
{
    nsresult rv = NS_OK;
    PR_CEnterMonitor(this);

    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    if (entry == NULL) {
        rv = NS_ERROR_SERVICE_NOT_FOUND;
    }
    else {
        rv = entry->NotifyListeners();         // break the cycles
        entry->mShuttingDown = PR_TRUE;
        nsrefcnt cnt;
        NS_RELEASE2(entry->mService, cnt);       // AddRef in GetService
        if (NS_SUCCEEDED(rv) && cnt == 0) {
            mServices->Remove(&key);
            delete entry;
            rv = nsComponentManager::FreeLibraries();
        }
        else
            rv = NS_ERROR_SERVICE_IN_USE;
    }

    PR_CExitMonitor(this);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// let's do it again, this time with ProgIDs...

NS_IMETHODIMP
nsServiceManagerImpl::RegisterService(const char* aProgID, nsISupports* aService)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ProgIDToCLSID(aProgID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return RegisterService(aClass, aService);
}

NS_IMETHODIMP
nsServiceManagerImpl::UnregisterService(const char* aProgID)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ProgIDToCLSID(aProgID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return UnregisterService(aClass);
}

NS_IMETHODIMP
nsServiceManagerImpl::GetService(const char* aProgID, const nsIID& aIID,
                                 nsISupports* *result,
                                 nsIShutdownListener* shutdownListener)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ProgIDToCLSID(aProgID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return GetService(aClass, aIID, result, shutdownListener);
}

NS_IMETHODIMP
nsServiceManagerImpl::ReleaseService(const char* aProgID, nsISupports* service,
                                     nsIShutdownListener* shutdownListener)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ProgIDToCLSID(aProgID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return ReleaseService(aClass, service, shutdownListener);
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewServiceManager(nsIServiceManager* *result)
{
    nsServiceManagerImpl* servMgr = new nsServiceManagerImpl();
    if (servMgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(servMgr);
    *result = servMgr;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Global service manager interface (see nsIServiceManager.h)

nsresult
nsServiceManager::GetGlobalServiceManager(nsIServiceManager* *result)
{
    nsresult rv = NS_OK;
    if (mGlobalServiceManager == NULL) {
        // XPCOM not initialized yet. Let us do initialization of our module.
        rv = NS_InitXPCOM(NULL);
    }
    // No ADDREF as we are advicing no release of this.
    if (NS_SUCCEEDED(rv)) *result = mGlobalServiceManager;

    return rv;
}

nsresult
nsServiceManager::GetService(const nsCID& aClass, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    nsIServiceManager* mgr;
    nsresult rv = GetGlobalServiceManager(&mgr);
    if (NS_FAILED(rv)) return rv;
    return mgr->GetService(aClass, aIID, result, shutdownListener);
}

nsresult
nsServiceManager::ReleaseService(const nsCID& aClass, nsISupports* service,
                                 nsIShutdownListener* shutdownListener)
{
    nsIServiceManager* mgr;
    nsresult rv = GetGlobalServiceManager(&mgr);
    if (NS_FAILED(rv)) return rv;
    return mgr->ReleaseService(aClass, service, shutdownListener);
}

nsresult
nsServiceManager::RegisterService(const nsCID& aClass, nsISupports* aService)
{
    nsIServiceManager* mgr;
    nsresult rv = GetGlobalServiceManager(&mgr);
    if (NS_FAILED(rv)) return rv;
    return mgr->RegisterService(aClass, aService);
}

nsresult
nsServiceManager::UnregisterService(const nsCID& aClass)
{
    nsIServiceManager* mgr;
    nsresult rv = GetGlobalServiceManager(&mgr);
    if (NS_FAILED(rv)) return rv;
    return mgr->UnregisterService(aClass);
}

////////////////////////////////////////////////////////////////////////////////

// XPCOM initialization
//
// To Control the order of initialization of these key components I am putting
// this function.
//
//	- nsServiceManager
//		- nsComponentManager
//			- nsRegistry
//
// Here are key points to remember:
//	- A global of all these need to exist. nsServiceManager is an independent object.
//	  nsComponentManager uses both the globalServiceManager and its own registry.
//
//	- A static object of both the nsComponentManager and nsServiceManager
//	  are in use. Hence InitXPCOM() gets triggered from both
//	  NS_GetGlobale{Service/Component}Manager() calls.
//
//	- There exists no global Registry. Registry can be created from the component manager.
//
#include "nsComponentManager.h"
#include "nsIRegistry.h"
#include "nsXPComCIID.h"
extern "C" NS_EXPORT nsresult
NS_RegistryGetFactory(nsISupports* servMgr, nsIFactory** aFactory);

nsIServiceManager* nsServiceManager::mGlobalServiceManager = NULL;
nsComponentManagerImpl* nsComponentManagerImpl::gComponentManager = NULL;

nsresult NS_InitXPCOM(nsIServiceManager* *result)
{
    nsresult rv = NS_OK;

    // 1. Create the Global Service Manager
    nsIServiceManager* servMgr = NULL;
    if (nsServiceManager::mGlobalServiceManager == NULL)
    {
        rv = NS_NewServiceManager(&servMgr);
        if (NS_FAILED(rv)) return rv;
        nsServiceManager::mGlobalServiceManager = servMgr;
        NS_ADDREF(servMgr);
        if (result && *result) *result = servMgr;
    }

    // 2. Create the Component Manager and register with global service manager
    //    It is understood that the component manager can use the global service manager.
    nsComponentManagerImpl *compMgr = NULL;
    if (nsComponentManagerImpl::gComponentManager == NULL)
    {
        compMgr = new nsComponentManagerImpl();
        if (compMgr == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(compMgr);
        nsresult rv = compMgr->Init();
        if (NS_FAILED(rv))
        {
            NS_RELEASE(compMgr);
            return rv;
        }
        nsComponentManagerImpl::gComponentManager = compMgr;
    }
    
    rv = servMgr->RegisterService(kComponentManagerCID, compMgr);
    if (NS_FAILED(rv))
    {
        return rv;
    }


    // 3. Register the RegistryFactory with the component manager so that
    //    clients can create new registry objects.
    nsIFactory *registryFactory = NULL;
    rv = NS_RegistryGetFactory(servMgr, &registryFactory);
    if (NS_FAILED(rv)) return (rv);

    NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);

    rv = compMgr->RegisterFactory(kRegistryCID, NS_REGISTRY_CLASSNAME,
                                  NS_REGISTRY_PROGID, registryFactory,
                                  PR_TRUE);
    NS_RELEASE(registryFactory);

   return (rv); 
}
