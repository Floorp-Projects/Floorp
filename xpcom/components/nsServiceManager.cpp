/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   IBM Corp.
 */

#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "prcmon.h"
#include "prthread.h" /* XXX: only used for the NSPR initialization hack (rick) */
#include "nsAutoLock.h"

nsIServiceManager* gServiceManager = NULL;
PRBool gShuttingDown = PR_FALSE;

nsresult
nsGetServiceByCID::operator()( const nsIID& aIID, void** aInstancePtr ) const
  {
    nsresult status;
    	// Too bad |nsServiceManager| isn't an |nsIServiceManager|, then this could have been one call
    if ( mServiceManager )
    	status = mServiceManager->GetService(mCID, aIID, NS_REINTERPRET_CAST(nsISupports**, aInstancePtr), 0);
    else
    	status = nsServiceManager::GetService(mCID, aIID, NS_REINTERPRET_CAST(nsISupports**, aInstancePtr), 0);

		if ( !NS_SUCCEEDED(status) )
			*aInstancePtr = 0;

    if ( mErrorPtr )
      *mErrorPtr = status;
    return status;
  }

nsresult
nsGetServiceByContractID::operator()( const nsIID& aIID, void** aInstancePtr ) const
  {
    nsresult status;
    if ( mContractID )
    	{
    			// Too bad |nsServiceManager| isn't an |nsIServiceManager|, then this could have been one call
    		if ( mServiceManager )
    			status = mServiceManager->GetService(mContractID, aIID, NS_REINTERPRET_CAST(nsISupports**, aInstancePtr), 0);
    		else
    			status = nsServiceManager::GetService(mContractID, aIID, NS_REINTERPRET_CAST(nsISupports**, aInstancePtr), 0);

        if ( !NS_SUCCEEDED(status) )
      		*aInstancePtr = 0;
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( mErrorPtr )
      *mErrorPtr = status;
    return status;
  }

nsresult
nsGetServiceFromCategory::operator()( const nsIID& aIID, void** aInstancePtr)
  const
{
  nsresult status;
  nsXPIDLCString value;
  nsCOMPtr<nsICategoryManager> catman = 
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &status);
  
  if (NS_FAILED(status)) goto error;
  
  if (!mCategory || !mEntry) {
    // when categories have defaults, use that for null mEntry
    status = NS_ERROR_NULL_POINTER;
    goto error;
  }
  
  /* find the contractID for category.entry */
  status = catman->GetCategoryEntry(mCategory, mEntry,
                                    getter_Copies(value));
  if (NS_FAILED(status)) goto error;
  if (!value) {
    status = NS_ERROR_SERVICE_NOT_FOUND;
    goto error;
  }
  
  // Too bad |nsServiceManager| isn't an |nsIServiceManager|, then
  // this could have been one call.
  if ( mServiceManager )
    status =
      mServiceManager->GetService(value, aIID,
                                  NS_REINTERPRET_CAST(nsISupports**,
                                                      aInstancePtr), 0);
  else
    status =
      nsServiceManager::GetService(value, aIID,
                                   NS_REINTERPRET_CAST(nsISupports**,
                                                       aInstancePtr), 0);
  if (NS_FAILED(status)) {
  error:
    *aInstancePtr = 0;
  }

  *mErrorPtr = status;
  return status;
}

class nsServiceEntry {
public:

    nsServiceEntry(const nsCID& cid, nsISupports* service);
    ~nsServiceEntry();

    nsresult AddListener(nsIShutdownListener* listener);
    nsresult RemoveListener(nsIShutdownListener* listener);
    nsresult NotifyListeners(void);

    const nsCID& mClassID;
    nsISupports* mService;
    nsVoidArray* mListeners;
    PRBool mShuttingDown;
};

nsServiceEntry::nsServiceEntry(const nsCID& cid, nsISupports* service)
    : mClassID(cid), mService(service), mListeners(NULL), mShuttingDown(PR_FALSE)
{
}

nsServiceEntry::~nsServiceEntry()
{
    if (mListeners) {
        NS_ASSERTION(mListeners->Count() == 0, "listeners not removed or notified");
#if 0
        PRUint32 size = mListeners->Count();
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
        mListeners = new nsVoidArray();
        if (mListeners == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    PRInt32 rv = mListeners->AppendElement(listener);
    NS_ADDREF(listener);
    return rv == -1 ? NS_ERROR_FAILURE : NS_OK;
}

nsresult
nsServiceEntry::RemoveListener(nsIShutdownListener* listener)
{
    if (listener == NULL)
        return NS_OK;
    NS_ASSERTION(mListeners, "no listeners added yet");
    if ( mListeners->RemoveElement(listener) )
    	return NS_OK;
    NS_ASSERTION(0, "unregistered shutdown listener");
    return NS_ERROR_FAILURE;
}

nsresult
nsServiceEntry::NotifyListeners(void)
{
    if (mListeners) {
        PRUint32 size = mListeners->Count();
        for (PRUint32 i = 0; i < size; i++) {
            nsIShutdownListener* listener = (nsIShutdownListener*)(*mListeners)[0];
            nsresult rv = listener->OnShutdown(mClassID, mService);
            if (NS_FAILED(rv)) return rv;
            NS_RELEASE(listener);
            mListeners->RemoveElementAt(0);
        }
        NS_ASSERTION(mListeners->Count() == 0, "failed to notify all listeners");
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
    RegisterService(const char* aContractID, nsISupports* aService);

    NS_IMETHOD
    UnregisterService(const char* aContractID);

    NS_IMETHOD
    GetService(const char* aContractID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const char* aContractID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    nsServiceManagerImpl(void);

    NS_DECL_ISUPPORTS

protected:

    virtual ~nsServiceManagerImpl(void);

    nsObjectHashtable/*<nsServiceEntry>*/* mServices;
    PRMonitor* mMonitor;
};

static PRBool PR_CALLBACK
DeleteEntry(nsHashKey *aKey, void *aData, void* closure)
{
    nsServiceEntry* entry = (nsServiceEntry*)aData;
    entry->NotifyListeners();
    NS_RELEASE(entry->mService);
    delete entry;
    return PR_TRUE;
}

nsServiceManagerImpl::nsServiceManagerImpl(void)
    : mMonitor(0)
{
    NS_INIT_REFCNT();
    mServices = new nsObjectHashtable(nsnull, nsnull,   // should never be cloned
                                      DeleteEntry, nsnull,
                                      256, PR_TRUE);    // Get a threadSafe hashtable
    NS_ASSERTION(mServices, "out of memory already?");

    /* XXX: This is a hack to force NSPR initialization..  This should be 
     *      removed once PR_CEnterMonitor(...) initializes NSPR... (rick)
     */
    (void)PR_GetCurrentThread();
    mMonitor = nsAutoMonitor::NewMonitor("nsServiceManagerImpl");
    NS_ASSERTION(mMonitor, "unable to get service manager monitor. Uh oh.");
}

nsServiceManagerImpl::~nsServiceManagerImpl(void)
{
    if (mServices) {
        delete mServices;
    }

    if (mMonitor) {
        nsAutoMonitor::DestroyMonitor(mMonitor);
        mMonitor = 0;
    }
}

NS_IMPL_ISUPPORTS1(nsServiceManagerImpl, nsIServiceManager)

NS_IMETHODIMP
nsServiceManagerImpl::GetService(const nsCID& aClass, const nsIID& aIID,
                                 nsISupports* *result,
                                 nsIShutdownListener* shutdownListener)
{
    nsAutoMonitor mon(mMonitor);

    // test this first, since there's no point in returning a service during
    // shutdown -- whether it's available or not would depend on the order it
    // occurs in the list
    if (gShuttingDown) {
        // When processing shutdown, dont process new GetService() requests
#ifdef DEBUG_dp
        NS_WARN_IF_FALSE(PR_FALSE, "Creating new service on shutdown. Denied.");
#endif /* DEBUG_dp */
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = NS_OK;
    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    if (entry) {
        nsISupports* service;
        rv = entry->mService->QueryInterface(aIID, (void**)&service);
        if (NS_SUCCEEDED(rv)) {
            // The refcount acquired in QI() above is "returned" to
            // the caller.

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
        return rv;
    }

    nsISupports* service;
    // We need to not be holding the service manager's monitor while calling 
    // CreateInstance, because it invokes user code which could try to re-enter
    // the service manager:
    mon.Exit();
    rv = nsComponentManager::CreateInstance(aClass, NULL, aIID, (void**)&service);
    mon.Enter();

    // If you hit this assertion, it means that someone tried (and
    // succeeded!) to get your service during your service's
    // initialization. This will mean that one instance of your
    // service will leak, and may mean that the client that
    // successfully acquired the service got a partially constructed
    // object.
    NS_ASSERTION(!mServices->Get(&key), "re-entrant call to service manager created service twice!");

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
                NS_ADDREF(service);		// Released in service manager destructor
            }
            else {
                NS_RELEASE(service);
                delete entry;
            }
        }
    }
    return rv;
}

NS_IMETHODIMP
nsServiceManagerImpl::ReleaseService(const nsCID& aClass, nsISupports* service,
                                     nsIShutdownListener* shutdownListener)
{
    PRBool serviceFound = PR_FALSE;
    nsresult rv = NS_OK;

#ifndef NS_DEBUG
    // Do entry lookup only if there is a shutdownlistener to be removed.
    //
    // For Debug builds, Consistency check for entry always. Releasing service
    // when the service is not with the servicemanager is mostly wrong.
    if (shutdownListener)
#endif
    {
        nsAutoMonitor mon(mMonitor);
        nsIDKey key(aClass);
        nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

        if (entry) {
            rv = entry->RemoveListener(shutdownListener);
            serviceFound = PR_TRUE;
        }
    }
    
    nsrefcnt cnt;
    NS_RELEASE2(service, cnt);

    // Consistency check: Service ref count cannot go to zero because of the
    // extra addref the service manager does, unless the service has been
    // unregistered (ie) not found in the service managers hash table.
    // 
    NS_ASSERTION(cnt > 0 || !serviceFound,
                 "*** Service in hash table but is being deleted. Dangling pointer\n"
                 "*** in service manager hash table.");

    return rv;
}

NS_IMETHODIMP
nsServiceManagerImpl::RegisterService(const nsCID& aClass, nsISupports* aService)
{
    nsresult rv = NS_OK;
    nsAutoMonitor mon(mMonitor);

    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);
    if (entry) {
        rv = NS_ERROR_FAILURE;
    }
    else {
        entry = new nsServiceEntry(aClass, aService);
        if (entry == NULL) 
            rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            mServices->Put(&key, entry);
            NS_ADDREF(aService);      // Released in DeleteEntry from UnregisterService
        }
    }
    return rv;
}

NS_IMETHODIMP
nsServiceManagerImpl::UnregisterService(const nsCID& aClass)
{
    nsresult rv = NS_OK;
    nsAutoMonitor mon(mMonitor);

    nsIDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    if (entry == NULL) {
        rv = NS_ERROR_SERVICE_NOT_FOUND;
    }
    else {
        rv = entry->NotifyListeners();  // break the cycles
        entry->mShuttingDown = PR_TRUE;
        mServices->RemoveAndDelete(&key);				// This will call the delete entry func
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// let's do it again, this time with ContractIDs...

NS_IMETHODIMP
nsServiceManagerImpl::RegisterService(const char* aContractID, nsISupports* aService)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ContractIDToClassID(aContractID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return RegisterService(aClass, aService);
}

NS_IMETHODIMP
nsServiceManagerImpl::UnregisterService(const char* aContractID)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ContractIDToClassID(aContractID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return UnregisterService(aClass);
}

NS_IMETHODIMP
nsServiceManagerImpl::GetService(const char* aContractID, const nsIID& aIID,
                                 nsISupports* *result,
                                 nsIShutdownListener* shutdownListener)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ContractIDToClassID(aContractID, &aClass);
    if (NS_FAILED(rv)) return rv;
    return GetService(aClass, aIID, result, shutdownListener);
}

NS_IMETHODIMP
nsServiceManagerImpl::ReleaseService(const char* aContractID, nsISupports* service,
                                     nsIShutdownListener* shutdownListener)
{
    nsCID aClass;
    nsresult rv;
    rv = nsComponentManager::ContractIDToClassID(aContractID, &aClass);
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
    if (gShuttingDown)
        return NS_ERROR_UNEXPECTED;

    nsresult rv = NS_OK;
    if (gServiceManager == NULL) {
        // XPCOM not initialized yet. Let us do initialization of our module.
        rv = NS_InitXPCOM(NULL, NULL);
    }
    // No ADDREF as we are advicing no release of this.
    if (NS_SUCCEEDED(rv))
        *result = gServiceManager;

    return rv;
}

nsresult
nsServiceManager::ShutdownGlobalServiceManager(nsIServiceManager* *result)
{
    if (gServiceManager != NULL) {
        nsrefcnt cnt;
        NS_RELEASE2(gServiceManager, cnt);
        NS_ASSERTION(cnt == 0, "Service Manager being held past XPCOM shutdown.");
        gServiceManager = NULL;
    }
    return NS_OK;
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
    // Don't create the global service manager here because we might be shutting
    // down, and releasing all the services in its destructor
    if (gServiceManager) 
        return gServiceManager->ReleaseService(aClass, service, shutdownListener);
    // If there wasn't a global service manager, just release the object:
    NS_RELEASE(service);
    return NS_OK;
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
// let's do it again, this time with ContractIDs...

nsresult
nsServiceManager::GetService(const char* aContractID, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    nsIServiceManager* mgr;
    nsresult rv = GetGlobalServiceManager(&mgr);
    if (NS_FAILED(rv)) return rv;
    return mgr->GetService(aContractID, aIID, result, shutdownListener);
}

nsresult
nsServiceManager::ReleaseService(const char* aContractID, nsISupports* service,
                                 nsIShutdownListener* shutdownListener)
{
    // Don't create the global service manager here because we might
    // be shutting down, and releasing all the services in its
    // destructor
    if (gServiceManager)
        return gServiceManager->ReleaseService(aContractID, service, shutdownListener);
    // If there wasn't a global service manager, just release the object:
    NS_RELEASE(service);
    return NS_OK;
}

nsresult
nsServiceManager::RegisterService(const char* aContractID, nsISupports* aService)
{
    nsIServiceManager* mgr;
    nsresult rv = GetGlobalServiceManager(&mgr);
    if (NS_FAILED(rv)) return rv;
    return mgr->RegisterService(aContractID, aService);
}

nsresult
nsServiceManager::UnregisterService(const char* aContractID)
{
    // Don't create the global service manager here because we might
    // be shutting down, and releasing all the services in its
    // destructor
    if (gServiceManager) 
        return gServiceManager->UnregisterService(aContractID);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
