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
#include "prcmon.h"
#include "prthread.h" /* XXX: only used for the NSPR initialization hack (rick) */

// XXX move to nsID.h or nsHashtable.h? (copied from nsRepository.cpp)
class IDKey: public nsHashKey {
private:
    nsID id;
  
public:
    IDKey(const nsID &aID) {
        id = aID;
    }
  
    PRUint32 HashValue(void) const {
        return id.m0;
    }

    PRBool Equals(const nsHashKey *aKey) const {
        return (id.Equals(((const IDKey *) aKey)->id));
    }

    nsHashKey *Clone(void) const {
        return new IDKey(id);
    }
};

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

};

nsServiceEntry::nsServiceEntry(const nsCID& cid, nsISupports* service)
    : mClassID(cid), mService(service), mListeners(NULL)
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
            listener->Release();
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
    PRInt32 err = mListeners->Add(listener);
    listener->AddRef();
    return err == -1 ? NS_ERROR_FAILURE : NS_OK;
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
            listener->Release();
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
            nsresult err = listener->OnShutdown(mClassID, mService);
            if (err) return err;
            listener->Release();
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
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ShutdownService(const nsCID& aClass);

    nsServiceManagerImpl(void);
    
    NS_DECL_ISUPPORTS

protected:

    virtual ~nsServiceManagerImpl(void);

    nsHashtable* mServices;         // nsHashtable<nsServiceEntry>
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
    entry->mService->Release();
    delete entry;
    return PR_TRUE;
}

nsServiceManagerImpl::~nsServiceManagerImpl(void)
{
    mServices->Enumerate(DeleteEntry);
    delete mServices;
}

static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMPL_ADDREF(nsServiceManagerImpl);
NS_IMPL_RELEASE(nsServiceManagerImpl);

nsresult
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

nsresult
nsServiceManagerImpl::GetService(const nsCID& aClass, const nsIID& aIID,
                                 nsISupports* *result,
                                 nsIShutdownListener* shutdownListener)
{
    nsresult err = NS_OK;
    /* XXX: This is a hack to force NSPR initialization..  This should be 
     *      removed once PR_CEnterMonitor(...) initializes NSPR... (rick)
     */
    (void)PR_GetCurrentThread();
    PR_CEnterMonitor(this);

    IDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    if (entry) {
        nsISupports* service;
        err = entry->mService->QueryInterface(aIID, (void**)&service);
        if (err == NS_OK) {
            err = entry->AddListener(shutdownListener);
            if (err == NS_OK) {
                *result = service;
            }
        }
    }
    else {
        nsISupports* service;
        err = nsRepository::CreateInstance(aClass, NULL, aIID, (void**)&service);
        if (err == NS_OK) {
            entry = new nsServiceEntry(aClass, service);
            if (entry == NULL) {
                service->Release();
                err = NS_ERROR_OUT_OF_MEMORY;
            }
            else {
                err = entry->AddListener(shutdownListener);
                if (err == NS_OK) {
                    mServices->Put(&key, entry);
                    service->AddRef(); // Add a extra ref so that the service is not freed for now.
                                       // Should fix it later depending on Mem pressure API or some
                                       // daemon killer thread.
                    *result = service;
                }
                else {
                    service->Release();
                    delete entry;
                }
            }
        }
    }

    PR_CExitMonitor(this);
    return err;
}

nsresult
nsServiceManagerImpl::ReleaseService(const nsCID& aClass, nsISupports* service,
                                 nsIShutdownListener* shutdownListener)
{
    nsresult err = NS_OK;
    PR_CEnterMonitor(this);

    IDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    NS_ASSERTION(entry, "service not found");
    NS_ASSERTION(entry->mService == service, "service looked failed");

    if (entry) {
        err = entry->RemoveListener(shutdownListener);
        // XXX Is this too aggressive? Maybe we should have a memory
        // pressure API that releases services if they're not in use so
        // that we don't thrash.(warren)
        nsrefcnt cnt = service->Release();
#if 0
       // Turns out to be too aggressive. Stanley reported that he always gets a new copy
       // of service.So do not remove it. Also one cannot depend return value of Release as per COM! (sudu)
        if (err == NS_OK && cnt == 0) {
            mServices->Remove(&key);
            delete entry;
        }
#endif 
    }

    PR_CExitMonitor(this);
    return err;
}

nsresult
nsServiceManagerImpl::ShutdownService(const nsCID& aClass)
{
    nsresult err = NS_OK;
    PR_CEnterMonitor(this);

    IDKey key(aClass);
    nsServiceEntry* entry = (nsServiceEntry*)mServices->Get(&key);

    if (entry == NULL) {
        err = NS_ERROR_SERVICE_NOT_FOUND;
    }
    else {
        entry->mService->AddRef();
        err = entry->NotifyListeners();
        nsrefcnt cnt = entry->mService->Release();
        if (err == NS_OK && cnt == 0) {
            mServices->Remove(&key);
            err = nsRepository::FreeLibraries();
        }
        else
            err = NS_ERROR_SERVICE_IN_USE;
    }

    PR_CExitMonitor(this);
    return err;
}

////////////////////////////////////////////////////////////////////////////////
// Global service manager interface (see nsIServiceManager.h)

nsIServiceManager* nsServiceManager::globalServiceManager = NULL;

nsresult
nsServiceManager::GetGlobalServiceManager(nsIServiceManager* *result)
{
    if (globalServiceManager == NULL) {
        globalServiceManager = new nsServiceManagerImpl();
        if (globalServiceManager == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
        globalServiceManager->AddRef();
    }
    *result = globalServiceManager;
    return NS_OK;
}

nsresult
nsServiceManager::GetService(const nsCID& aClass, const nsIID& aIID,
                             nsISupports* *result,
                             nsIShutdownListener* shutdownListener)
{
    nsIServiceManager* mgr;
    nsresult rslt = GetGlobalServiceManager(&mgr);
    if (rslt != NS_OK) return rslt;
    return mgr->GetService(aClass, aIID, result, shutdownListener);
}

nsresult
nsServiceManager::ReleaseService(const nsCID& aClass, nsISupports* service,
                                 nsIShutdownListener* shutdownListener)
{
    nsIServiceManager* mgr;
    nsresult rslt = GetGlobalServiceManager(&mgr);
    if (rslt != NS_OK) return rslt;
    return mgr->ReleaseService(aClass, service, shutdownListener);
}

nsresult
nsServiceManager::ShutdownService(const nsCID& aClass)
{
    nsIServiceManager* mgr;
    nsresult rslt = GetGlobalServiceManager(&mgr);
    if (rslt != NS_OK) return rslt;
    return mgr->ShutdownService(aClass);
}

////////////////////////////////////////////////////////////////////////////////
