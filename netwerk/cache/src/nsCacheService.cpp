/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is nsCacheService.cpp, released February 10, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 10-February-2001
 */


#include "nsCache.h"
#include "nsCacheService.h"
#include "nsCacheRequest.h"
#include "nsCacheEntry.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheDevice.h"
#include "nsDiskCacheDevice.h"
#include "nsMemoryCacheDevice.h"
#include "nsICacheVisitor.h"

#include "nsAutoLock.h"
#include "nsIEventQueue.h"
#include "nsIObserverService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPref.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsVoidArray.h"



/******************************************************************************
 * nsCacheProfilePrefObserver
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark nsCacheProfilePrefObserver
#endif

#define DISK_CACHE_ENABLE_PREF      "browser.cache.disk.enable"
#define DISK_CACHE_DIR_PREF         "browser.cache.disk.parent_directory"
#define DISK_CACHE_CAPACITY_PREF    "browser.cache.disk.capacity"

#define MEMORY_CACHE_ENABLE_PREF    "browser.cache.memory.enable"
#define MEMORY_CACHE_CAPACITY_PREF  "browser.cache.memory.capacity"


class nsCacheProfilePrefObserver : public nsIObserver
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    nsCacheProfilePrefObserver()
        : mHaveProfile(PR_FALSE)
        , mDiskCacheEnabled(PR_FALSE)
        , mDiskCacheCapacity(0)
        , mMemoryCacheEnabled(PR_TRUE)
        , mMemoryCacheCapacity(4 * 1024 * 1024)
    {
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsCacheProfilePrefObserver() {}
    
    nsresult        Install();
    nsresult        Remove();
    nsresult        ReadPrefs();
    
    PRBool          DiskCacheEnabled();
    PRInt32         DiskCacheCapacity()         { return mDiskCacheCapacity; }
    nsILocalFile *  DiskCacheParentDirectory()  { return mDiskCacheParentDirectory; }
    
    PRBool          MemoryCacheEnabled();
    PRInt32         MemoryCacheCapacity()       { return mMemoryCacheCapacity; }

private:
    PRBool                  mHaveProfile;
    
    PRBool                  mDiskCacheEnabled;
    PRInt32                 mDiskCacheCapacity;
    nsCOMPtr<nsILocalFile>  mDiskCacheParentDirectory;
    
    PRBool                  mMemoryCacheEnabled;
    PRInt32                 mMemoryCacheCapacity;
};

NS_IMPL_ISUPPORTS1(nsCacheProfilePrefObserver, nsIObserver);


nsresult
nsCacheProfilePrefObserver::Install()
{
    nsresult rv, rv2 = NS_OK;
    
    // install profile-change observer
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    NS_ENSURE_ARG(observerService);
    
    rv = observerService->AddObserver(this, "profile-before-change", PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;
    
    rv = observerService->AddObserver(this, "profile-after-change", PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;


    // install xpcom shutdown observer
    rv = observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;
    
    
    // install preferences observer
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrefBranchInternal> prefInternal = do_QueryInterface(prefService, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefInternal->AddObserver(MEMORY_CACHE_ENABLE_PREF, this, PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;

    rv = prefInternal->AddObserver(DISK_CACHE_ENABLE_PREF, this, PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;
    
    rv = prefInternal->AddObserver(DISK_CACHE_DIR_PREF, this, PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;
    
    rv = prefInternal->AddObserver(DISK_CACHE_CAPACITY_PREF, this, PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;
    
    rv = prefInternal->AddObserver(MEMORY_CACHE_CAPACITY_PREF, this, PR_FALSE);
    if (NS_FAILED(rv)) rv2 = rv;
    
    rv = ReadPrefs();
    
    return NS_SUCCEEDED(rv) ? rv2 : rv;
}


nsresult
nsCacheProfilePrefObserver::Remove()
{
    nsresult rv, rv2 = NS_OK;

    // remove Observer Service observers
    nsCOMPtr<nsIObserverService> observerService = do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    NS_ENSURE_ARG(observerService);

    rv = observerService->RemoveObserver(this, "profile-before-change");
    if (NS_FAILED(rv)) rv2 = rv;
    
    rv = observerService->RemoveObserver(this, "profile-after-change");
    if (NS_FAILED(rv)) rv2 = rv;

    rv = observerService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    if (NS_FAILED(rv)) rv2 = rv;


    // remove Pref Service observers
    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPrefBranchInternal> prefInternal = do_QueryInterface(prefService, &rv);
    if (NS_FAILED(rv)) return rv;

    // remove Disk cache pref observers
    rv  = prefInternal->RemoveObserver(DISK_CACHE_ENABLE_PREF, this);
    if (NS_FAILED(rv)) rv2 = rv;

    rv  = prefInternal->RemoveObserver(DISK_CACHE_CAPACITY_PREF, this);
    if (NS_FAILED(rv)) rv2 = rv;

    rv  = prefInternal->RemoveObserver(DISK_CACHE_DIR_PREF, this);
    if (NS_FAILED(rv)) rv2 = rv;
    
    // remove Memory cache pref observers
    rv = prefInternal->RemoveObserver(MEMORY_CACHE_ENABLE_PREF, this);
    if (NS_FAILED(rv)) rv2 = rv;

    rv = prefInternal->RemoveObserver(MEMORY_CACHE_CAPACITY_PREF, this);
    // if (NS_FAILED(rv)) rv2 = rv;

    return NS_SUCCEEDED(rv) ? rv2 : rv;
}


NS_IMETHODIMP
nsCacheProfilePrefObserver::Observe(nsISupports *     subject,
                                    const char * topic,
                                    const PRUnichar * data)
{
#ifdef DEBUG
    printf("### nsCacheProfilePrefObserver::Observe [topic=%s data=%s]\n", topic, 
           NS_ConvertUCS2toUTF8(data).get());
#endif
            
    nsresult rv;
    if (!nsCRT::strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, topic)) {
        // xpcom going away, shutdown cache service
        if (nsCacheService::GlobalInstance())
            nsCacheService::GlobalInstance()->Shutdown();
    
    } else if (!nsCRT::strcmp("profile-before-change", topic)) {
        // profile before change
        mHaveProfile = PR_FALSE;

        // XXX shutdown devices
        nsCacheService::OnProfileShutdown(!nsCRT::strcmp("shutdown-cleanse", NS_ConvertUCS2toUTF8(data).get()));
        
    } else if (!nsCRT::strcmp("profile-after-change", topic)) {
        // profile after change
        mHaveProfile = PR_TRUE;
        ReadPrefs();
        nsCacheService::OnProfileChanged();
    
    } else if (!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, topic)) {
        if (!mHaveProfile)  return NS_OK;
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(subject, &rv);
        if (NS_FAILED(rv))
            return rv;

        // which preference changed?
        if (!nsCRT::strcmp(DISK_CACHE_ENABLE_PREF, NS_ConvertUCS2toUTF8(data).get())) {

            rv = prefBranch->GetBoolPref(DISK_CACHE_ENABLE_PREF, &mDiskCacheEnabled);
            if (NS_FAILED(rv))  return rv;
            nsCacheService::SetDiskCacheEnabled(DiskCacheEnabled());
            

        } else if (!nsCRT::strcmp(DISK_CACHE_CAPACITY_PREF, NS_ConvertUCS2toUTF8(data).get())) {

            PRInt32 capacity = 0;
            rv = prefBranch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &capacity);
            if (NS_FAILED(rv))  return rv;
            mDiskCacheCapacity = (PRUint32) PR_MAX(0, capacity);
            nsCacheService::SetDiskCacheCapacity(mDiskCacheCapacity);
#if 0            
        } else if (!nsCRT::strcmp(DISK_CACHE_DIR_PREF, NS_ConvertUCS2toUTF8(data).get())) {
            // XXX we probaby don't want to respond to this pref except after profile changes
            // XXX ideally, there should be somekind of user notification that the pref change
            // XXX won't take effect until the next time the profile changes (browser launch)
#endif            
        } else if (!nsCRT::strcmp(MEMORY_CACHE_ENABLE_PREF, NS_ConvertUCS2toUTF8(data).get())) {

            rv = prefBranch->GetBoolPref(MEMORY_CACHE_ENABLE_PREF, &mMemoryCacheEnabled);
            if (NS_FAILED(rv))  return rv;
            nsCacheService::SetMemoryCacheEnabled(MemoryCacheEnabled());
            
        } else if (!nsCRT::strcmp(MEMORY_CACHE_CAPACITY_PREF, NS_ConvertUCS2toUTF8(data).get())) {

            PRInt32 capacity = 0;
            rv = prefBranch->GetIntPref(MEMORY_CACHE_CAPACITY_PREF, &capacity);
            if (NS_FAILED(rv))  return rv;
            mMemoryCacheCapacity = (PRUint32) PR_MAX(0, capacity);
            nsCacheService::SetMemoryCacheCapacity(mMemoryCacheCapacity);
        }
    }
    
    return NS_OK;
}


nsresult
nsCacheProfilePrefObserver::ReadPrefs()
{
    nsresult rv, rv2 = NS_OK;
    PRInt32 capacity = 0;

    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(prefService, &rv);
    if (NS_FAILED(rv))  return rv;

    // read disk cache device prefs
    rv = prefBranch->GetBoolPref(DISK_CACHE_ENABLE_PREF, &mDiskCacheEnabled);
    if (NS_FAILED(rv)) rv2 = rv;

    rv = prefBranch->GetIntPref(DISK_CACHE_CAPACITY_PREF, &capacity);
    if (NS_FAILED(rv)) rv2 = rv;
    mDiskCacheCapacity = (PRUint32) PR_MAX(0, capacity);

    (void) prefBranch->GetComplexValue(DISK_CACHE_DIR_PREF,     // ignore error
                                     NS_GET_IID(nsILocalFile),
                                     getter_AddRefs(mDiskCacheParentDirectory));
    
    if (!mDiskCacheParentDirectory) {
        nsCOMPtr<nsIFile>  directory;
        // try to get the profile directory (there may not be a profile yet)
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(directory));
#if DEBUG
        if (NS_FAILED(rv)) {
            // use current process directory during development
            rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, getter_AddRefs(directory));
        }
#endif
        if (directory)
            mDiskCacheParentDirectory = do_QueryInterface(directory, &rv);
    }
    
    // read memory cache device prefs
    rv = prefBranch->GetBoolPref(MEMORY_CACHE_ENABLE_PREF, &mMemoryCacheEnabled);
    if (NS_FAILED(rv)) rv2 = rv;
    
    capacity = 0;
    rv = prefBranch->GetIntPref(MEMORY_CACHE_CAPACITY_PREF, &capacity);
    mMemoryCacheCapacity = (PRUint32) PR_MAX(0, capacity);
        
    return NS_SUCCEEDED(rv) ? rv2 : rv;
}


PRBool
nsCacheProfilePrefObserver::DiskCacheEnabled()
{
    if ((mDiskCacheCapacity == 0) || (!mDiskCacheParentDirectory))  return PR_FALSE;
    return mDiskCacheEnabled;
}

    
PRBool
nsCacheProfilePrefObserver::MemoryCacheEnabled()
{
    if (mMemoryCacheCapacity == 0)  return PR_FALSE;
    return mMemoryCacheEnabled;
}



/******************************************************************************
 * nsCacheService
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark -
#pragma mark nsCacheService
#endif

nsCacheService *   nsCacheService::gService = nsnull;

//NS_IMPL_THREADSAFE_ISUPPORTS2(nsCacheService, nsICacheService, nsIObserver)
NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheService, nsICacheService);

nsCacheService::nsCacheService()
    : mCacheServiceLock(nsnull),
      mEnableMemoryDevice(PR_TRUE),
      mEnableDiskDevice(PR_TRUE),
      mMemoryDevice(nsnull),
      mDiskDevice(nsnull),
      mTotalEntries(0),
      mCacheHits(0),
      mCacheMisses(0),
      mMaxKeyLength(0),
      mMaxDataSize(0),
      mMaxMetaSize(0),
      mDeactivateFailures(0),
      mDeactivatedUnboundEntries(0)
{
  NS_INIT_REFCNT();

  NS_ASSERTION(gService==nsnull, "multiple nsCacheService instances!");
  gService = this;

  // create list of cache devices
  PR_INIT_CLIST(&mDoomedEntries);
}

nsCacheService::~nsCacheService()
{
    if (mCacheServiceLock) // Shutdown hasn't been called yet.
        (void) Shutdown();

    gService = nsnull;
}


NS_IMETHODIMP
nsCacheService::Init()
{
    nsresult  rv;

    NS_ASSERTION(mCacheServiceLock== nsnull, "nsCacheService already initialized.");
    if (mCacheServiceLock)
        return NS_ERROR_ALREADY_INITIALIZED;

    CACHE_LOG_INIT();

    mCacheServiceLock = PR_NewLock();
    if (mCacheServiceLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    // initialize hashtable for active cache entries
    rv = mActiveEntries.Init();
    if (NS_FAILED(rv)) goto error;
    
    // get references to services we'll be using frequently
    mEventQService = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    mProxyObjectManager = do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    mObserver = new nsCacheProfilePrefObserver();
    if (!mObserver)  return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mObserver);
    
    mObserver->Install();
    mEnableDiskDevice =   mObserver->DiskCacheEnabled();
    mEnableMemoryDevice = mObserver->MemoryCacheEnabled();


    rv = CreateMemoryDevice();
    if (NS_FAILED(rv) && (rv != NS_ERROR_NOT_AVAILABLE))  goto error;
    
    return NS_OK;

 error:
    (void)Shutdown();

    return rv;
}


NS_IMETHODIMP
nsCacheService::Shutdown()
{
    NS_ASSERTION(mCacheServiceLock != nsnull, 
                 "can't shutdown nsCacheService unless it has been initialized.");

    if (mCacheServiceLock) {
        // XXX this is not sufficient
        PRLock * tempLock = mCacheServiceLock;
        mCacheServiceLock = nsnull;

#if defined(PR_LOGGING)
        LogCacheStatistics();
#endif

        mObserver->Remove();
        NS_RELEASE(mObserver);
        
        // Clear entries
        ClearDoomList();
        ClearActiveEntries();

        // deallocate memory and disk caches
        delete mMemoryDevice;
        mMemoryDevice = nsnull;

        delete mDiskDevice;
        mDiskDevice = nsnull;

        PR_DestroyLock(tempLock);
    }
    return NS_OK;
}


NS_METHOD
nsCacheService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsresult  rv;

    if (aOuter != nsnull)
        return NS_ERROR_NO_AGGREGATION;

    nsCacheService * cacheService = new nsCacheService();
    if (cacheService == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(cacheService);
    rv = cacheService->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = cacheService->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(cacheService);
    return rv;
}


NS_IMETHODIMP
nsCacheService::CreateSession(const char *          clientID,
                              nsCacheStoragePolicy  storagePolicy, 
                              PRBool                streamBased,
                              nsICacheSession     **result)
{
    *result = nsnull;

    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;

    nsCacheSession * session = new nsCacheSession(clientID, storagePolicy, streamBased);
    if (!session)  return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = session);

    return NS_OK;
}


nsresult
nsCacheService::EvictEntriesForSession(nsCacheSession * session)
{
    return EvictEntriesForClient(session->ClientID()->get(),
                                 session->StoragePolicy());
}


nsresult
nsCacheService::EvictEntriesForClient(const char *          clientID,
                                      nsCacheStoragePolicy  storagePolicy)
{
    if (this == nsnull) return NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mCacheServiceLock);
    nsresult rv = NS_OK;

    if (storagePolicy == nsICache::STORE_ANYWHERE ||
        storagePolicy == nsICache::STORE_ON_DISK) {

        if (mEnableDiskDevice) {
            if (!mDiskDevice) {
                rv = CreateDiskDevice();
                if (NS_FAILED(rv)) return rv;
            }
            rv = mDiskDevice->EvictEntries(clientID);
            if (NS_FAILED(rv)) return rv;
        }
    }

    if (storagePolicy == nsICache::STORE_ANYWHERE ||
        storagePolicy == nsICache::STORE_IN_MEMORY) {

        if (mEnableMemoryDevice) {
            rv = mMemoryDevice->EvictEntries(clientID);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP nsCacheService::VisitEntries(nsICacheVisitor *visitor)
{
    nsAutoLock lock(mCacheServiceLock);

    if (!(mEnableDiskDevice || mEnableMemoryDevice))
        return NS_ERROR_NOT_AVAILABLE;

    // XXX record the fact that a visitation is in progress, i.e. keep
    // list of visitors in progress.
    
    nsresult rv = NS_OK;
    if (mEnableMemoryDevice) {
        rv = mMemoryDevice->Visit(visitor);
        if (NS_FAILED(rv)) return rv;
    }

    if (mEnableDiskDevice) {
        if (!mDiskDevice) {
            rv = CreateDiskDevice();
            if (NS_FAILED(rv)) return rv;
        }
        rv = mDiskDevice->Visit(visitor);
        if (NS_FAILED(rv)) return rv;
    }

    // XXX notify any shutdown process that visitation is complete for THIS visitor.
    // XXX keep queue of visitors

    return NS_OK;
}


NS_IMETHODIMP nsCacheService::EvictEntries(nsCacheStoragePolicy storagePolicy)
{
    return  EvictEntriesForClient(nsnull, storagePolicy);
}


/**
 * Internal Methods
 */
nsresult
nsCacheService::CreateDiskDevice()
{
    if (!mEnableDiskDevice) return NS_ERROR_NOT_AVAILABLE;
    if (mDiskDevice)        return NS_OK;

    mDiskDevice = new nsDiskCacheDevice;
    if (!mDiskDevice)       return NS_ERROR_OUT_OF_MEMORY;

    // set the preferences
    mDiskDevice->SetCacheParentDirectory(mObserver->DiskCacheParentDirectory());
    mDiskDevice->SetCapacity(mObserver->DiskCacheCapacity());
    
    nsresult rv = mDiskDevice->Init();
    if (NS_FAILED(rv)) {
#if DEBUG
        printf("###\n");
        printf("### mDiskDevice->Init() failed (0x%.8x)\n", rv);
        printf("###    - disabling disk cache for this session.\n");
        printf("###\n");
#endif        
        mEnableDiskDevice = PR_FALSE;
        delete mDiskDevice;
        mDiskDevice = nsnull;
    }
    return rv;
}


nsresult
nsCacheService::CreateMemoryDevice()
{
    if (!mEnableMemoryDevice) return NS_ERROR_NOT_AVAILABLE;
    if (mMemoryDevice)        return NS_OK;

    mMemoryDevice = new nsMemoryCacheDevice;
    if (!mMemoryDevice)       return NS_ERROR_OUT_OF_MEMORY;
    
    // set preference
    mMemoryDevice->SetCapacity(mObserver->MemoryCacheCapacity());

    nsresult rv = mMemoryDevice->Init();
    if (NS_FAILED(rv)) {
        // XXX log error
        delete mMemoryDevice;
        mMemoryDevice = nsnull;
    }
    return rv;
}


nsresult
nsCacheService::CreateRequest(nsCacheSession *   session,
                              const char *       clientKey,
                              nsCacheAccessMode  accessRequested,
                              PRBool             blockingMode,
                              nsICacheListener * listener,
                              nsCacheRequest **  request)
{
    NS_ASSERTION(request, "CommonOpenCacheEntry: request or entry is null");
     
    nsCString * key = new nsCString(*session->ClientID());
    if (!key)
        return NS_ERROR_OUT_OF_MEMORY;
    key->Append(":");
    key->Append(clientKey);

    if (mMaxKeyLength < key->Length()) mMaxKeyLength = key->Length();

    // create request
    *request = new  nsCacheRequest(key, listener, accessRequested, blockingMode, session);    
    if (!*request) {
        delete key;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!listener)  return NS_OK;  // we're sync, we're done.

    // get the nsIEventQueue for the request's thread
    (*request)->mThread = PR_GetCurrentThread();
    
    return NS_OK;
}


nsresult
nsCacheService::NotifyListener(nsCacheRequest *          request,
                               nsICacheEntryDescriptor * descriptor,
                               nsCacheAccessMode         accessGranted,
                               nsresult                  error)
{
    nsresult rv;

    nsCOMPtr<nsICacheListener> listenerProxy;
    NS_ASSERTION(request->mThread, "no thread set in async request!");
    nsCOMPtr<nsIEventQueue> eventQ;
    mEventQService->GetThreadEventQueue(request->mThread,
                                        getter_AddRefs(eventQ));
    rv = mProxyObjectManager->GetProxyForObject(eventQ,
                                                NS_GET_IID(nsICacheListener),
                                                request->mListener,
                                                PROXY_ASYNC|PROXY_ALWAYS,
                                                getter_AddRefs(listenerProxy));
    if (NS_FAILED(rv)) return rv;

    return listenerProxy->OnCacheEntryAvailable(descriptor, accessGranted, error);
}


nsresult
nsCacheService::ProcessRequest(nsCacheRequest *           request,
                               PRBool                     calledFromOpenCacheEntry,
                               nsICacheEntryDescriptor ** result)
{
    // !!! must be called with mCacheServiceLock held !!!
    nsresult           rv;
    nsCacheEntry *     entry = nsnull;
    nsCacheAccessMode  accessGranted = nsICache::ACCESS_NONE;
    if (result) *result = nsnull;

    while(1) {  // Activate entry loop
        rv = ActivateEntry(request, &entry);  // get the entry for this request
        if (NS_FAILED(rv))  break;

        while(1) { // Request Access loop
            NS_ASSERTION(entry, "no entry in Request Access loop!");
            // entry->RequestAccess queues request on entry
            rv = entry->RequestAccess(request, &accessGranted);
            if (rv != NS_ERROR_CACHE_WAIT_FOR_VALIDATION) break;
            
            if (request->mListener) // async exits - validate, doom, or close will resume
                return rv;
            
            if (request->IsBlocking()) {
                PR_Unlock(mCacheServiceLock);
                rv = request->WaitForValidation();
                PR_Lock(mCacheServiceLock);
            }

            PR_REMOVE_AND_INIT_LINK(request);
            if (NS_FAILED(rv)) break;   // non-blocking mode returns WAIT_FOR_VALIDATION error
            // okay, we're ready to process this request, request access again
        }
        if (rv != NS_ERROR_CACHE_ENTRY_DOOMED)  break;

        if (entry->IsNotInUse()) {
            // this request was the last one keeping it around, so get rid of it
            DeactivateEntry(entry);
        }
        // loop back around to look for another entry
    }

    nsCOMPtr<nsICacheEntryDescriptor> descriptor;
    
    if (NS_SUCCEEDED(rv))
        rv = entry->CreateDescriptor(request, accessGranted, getter_AddRefs(descriptor));

    if (request->mListener) {  // Asynchronous
    
        if (NS_FAILED(rv) && calledFromOpenCacheEntry)
            return rv;  // skip notifying listener, just return rv to caller
            
        // call listener to report error or descriptor
        nsresult rv2 = NotifyListener(request, descriptor, accessGranted, rv);
        if (NS_FAILED(rv2) && NS_SUCCEEDED(rv)) {
            rv = rv2;  // trigger delete request
        }
    } else {        // Synchronous
        NS_IF_ADDREF(*result = descriptor);
    }
    return rv;
}


nsresult
nsCacheService::OpenCacheEntry(nsCacheSession *           session,
                                const char *               key,
                                nsCacheAccessMode          accessRequested,
                                PRBool                     blockingMode,
                                nsICacheListener *         listener,
                                nsICacheEntryDescriptor ** result)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;
    if (result)
        *result = nsnull;

    nsCacheRequest * request = nsnull;

    nsAutoLock lock(mCacheServiceLock);
    nsresult rv = CreateRequest(session,
                                key,
                                accessRequested,
                                blockingMode,
                                listener,
                                &request);
    if (NS_FAILED(rv))  return rv;

    rv = ProcessRequest(request, PR_TRUE, result);

    // delete requests that have completed
    if (!(listener && (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)))
        delete request;

    return rv;
}


nsresult
nsCacheService::ActivateEntry(nsCacheRequest * request, 
                              nsCacheEntry ** result)
{
    nsresult        rv = NS_OK;

    NS_ASSERTION(request != nsnull, "ActivateEntry called with no request");
    if (result) *result = nsnull;
    if ((!request) || (!result))  return NS_ERROR_NULL_POINTER;

    if (!mEnableMemoryDevice && (!mEnableDiskDevice || !request->IsStreamBased() ))
        return NS_ERROR_FAILURE;


    // search active entries (including those not bound to device)
    nsCacheEntry *entry = mActiveEntries.GetEntry(request->mKey);

    if (!entry) {
        // search cache devices for entry
        entry = SearchCacheDevices(request->mKey, request->StoragePolicy());
        if (entry)  entry->MarkInitialized();
    }

    if (entry) {
        ++mCacheHits;
        entry->Fetched();
    } else {
        ++mCacheMisses;
    }

    if (entry &&
        ((request->AccessRequested() == nsICache::ACCESS_WRITE) ||
         (entry->mExpirationTime &&
          entry->mExpirationTime <= SecondsFromPRTime(PR_Now()) &&
          request->WillDoomEntriesIfExpired())))
    {
        // this is FORCE-WRITE request or the entry has expired
        rv = DoomEntry_Locked(entry);
        if (NS_FAILED(rv)) {
            // XXX what to do?  Increment FailedDooms counter?
        }
        entry = nsnull;
    }

    if (!entry) {
		if (! (request->AccessRequested() & nsICache::ACCESS_WRITE)) {
			// this is a READ-ONLY request
		    rv = NS_ERROR_CACHE_KEY_NOT_FOUND;
			goto error;
		}

        entry = new nsCacheEntry(request->mKey,
                                 request->IsStreamBased(),
                                 request->StoragePolicy());
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        
        entry->Fetched();
        ++mTotalEntries;
        
        // XXX  we could perform an early bind in some cases based on storage policy
    }

    if (!entry->IsActive()) {
        rv = mActiveEntries.AddEntry(entry);
        if (NS_FAILED(rv)) goto error;
        entry->MarkActive();  // mark entry active, because it's now in mActiveEntries
    }
    *result = entry;
    return NS_OK;
    
 error:
    *result = nsnull;
    if (entry) {
        // XXX clean up
        delete entry;
    }
    return rv;
}


nsCacheEntry *
nsCacheService::SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy)
{
    nsCacheEntry * entry = nsnull;

    if ((policy == nsICache::STORE_ANYWHERE) || (policy == nsICache::STORE_IN_MEMORY)) {
        if (mEnableMemoryDevice)
            entry = mMemoryDevice->FindEntry(key);
    }

    if (!entry && 
        ((policy == nsICache::STORE_ANYWHERE) || (policy == nsICache::STORE_ON_DISK))) {

        if (mEnableDiskDevice) {
            if (!mDiskDevice) {
                nsresult rv = CreateDiskDevice();
                if (NS_FAILED(rv))
                    return nsnull;
            }
            
            entry = mDiskDevice->FindEntry(key);
        }
    }

    return entry;
}


nsCacheDevice *
nsCacheService::EnsureEntryHasDevice(nsCacheEntry * entry)
{
    nsCacheDevice * device = entry->CacheDevice();
    if (device)  return device;
    nsresult rv = NS_OK;

    if (entry->IsStreamData() && entry->IsAllowedOnDisk() && mEnableDiskDevice) {
        // this is the default
        if (!mDiskDevice) {
            rv = CreateDiskDevice();  // ignore the error (check for mDiskDevice instead)
        }

        if (mDiskDevice) {
            entry->MarkBinding();  // XXX
            rv = mDiskDevice->BindEntry(entry);
            entry->ClearBinding(); // XXX
            if (NS_SUCCEEDED(rv))
                device = mDiskDevice;
        }
    }
     
    // if we can't use mDiskDevice, try mMemoryDevice
    if (!device && mEnableMemoryDevice) {
        NS_ASSERTION(entry->IsAllowedInMemory(), "oops.. bad flags");
        
        entry->MarkBinding();  // XXX
        rv = mMemoryDevice->BindEntry(entry);
        entry->ClearBinding(); // XXX
        if (NS_SUCCEEDED(rv))
            device = mMemoryDevice;
    }

    if (device == nsnull)  return nsnull;

    entry->SetCacheDevice(device);
    return device;
}


nsresult
nsCacheService::ValidateEntry(nsCacheEntry * entry)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;

    nsAutoLock lock(mCacheServiceLock);
    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here

    entry->MarkValid();
    nsresult rv = ProcessPendingRequests(entry);
    NS_ASSERTION(rv == NS_OK, "ProcessPendingRequests failed.");
    // XXX what else can be done?

    return rv;
}


nsresult
nsCacheService::DoomEntry(nsCacheEntry * entry)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mCacheServiceLock);
    return DoomEntry_Locked(entry);
}


nsresult
nsCacheService::DoomEntry_Locked(nsCacheEntry * entry)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;
    if (entry->IsDoomed())  return NS_OK;
    
    nsresult  rv = NS_OK;
    entry->MarkDoomed();
    
    NS_ASSERTION(!entry->IsBinding(), "Dooming entry while binding device.");
    nsCacheDevice * device = entry->CacheDevice();
    if (device)  device->DoomEntry(entry);

    if (entry->IsActive()) {
        // remove from active entries
        mActiveEntries.RemoveEntry(entry);
        entry->MarkInactive();
     }

    // put on doom list to wait for descriptors to close
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry), "doomed entry still on device list");
    PR_APPEND_LINK(entry, &mDoomedEntries);

    // tell pending requests to get on with their lives...
    rv = ProcessPendingRequests(entry);
    
    // All requests have been removed, but there may still be open descriptors
    if (entry->IsNotInUse()) {
        DeactivateEntry(entry); // tell device to get rid of it
    }
    return rv;
}


static void* PR_CALLBACK
EventHandler(PLEvent *self)
{
    nsISupports * object = (nsISupports *)PL_GetEventOwner(self);
    NS_RELEASE(object);
    return 0;
}


static void PR_CALLBACK
DestroyHandler(PLEvent *self)
{
    delete self;
}


void
nsCacheService::ProxyObjectRelease(nsISupports * object, PRThread * thread)
{
    NS_ASSERTION(gService, "nsCacheService not initialized");
    NS_ASSERTION(thread, "no thread");
    // XXX if thread == current thread, we could avoid posting an event,
    // XXX by add this object to a queue and release it when the cache service is unlocked.
    
    nsCOMPtr<nsIEventQueue> eventQ;
    gService->mEventQService->GetThreadEventQueue(thread, getter_AddRefs(eventQ));
    NS_ASSERTION(eventQ, "no event queue for thread");
    if (!eventQ)  return;
    
    PLEvent * event = new PLEvent;
    if (!event) {
        // XXX warning
        return;
    }
    PL_InitEvent(event, object, EventHandler, DestroyHandler);
    eventQ->PostEvent(event);
}


void
nsCacheService::OnProfileShutdown(PRBool cleanse)
{
    if (!gService)  return;
    nsAutoLock lock(gService->mCacheServiceLock);

    if (gService->mDiskDevice) {
        if (cleanse)
            gService->mDiskDevice->EvictEntries(nsnull);

        gService->DoomActiveEntries();
        gService->ClearDoomList();
        
        gService->mDiskDevice->Shutdown();
        gService->mEnableDiskDevice = PR_FALSE;
    }
#if 0
    if (gService->mMemoryDevice) {
        gService->mMemoryDevice->Shutdown();
        gService->mEnableMemoryDevice = PR_FALSE;
    }
#endif
}


void
nsCacheService::OnProfileChanged()
{
    if (!gService)  return;
 
    nsresult   rv = NS_OK;
    nsAutoLock lock(gService->mCacheServiceLock);
    
    gService->mEnableDiskDevice   = gService->mObserver->DiskCacheEnabled();
    gService->mEnableMemoryDevice = gService->mObserver->MemoryCacheEnabled();

    if (gService->mDiskDevice) {
        gService->mDiskDevice->SetCacheParentDirectory(gService->mObserver->DiskCacheParentDirectory());
        gService->mDiskDevice->SetCapacity(gService->mObserver->DiskCacheCapacity());

        // XXX initialization of mDiskDevice could be made lazily, if mEnableDiskDevice is false
        rv = gService->mDiskDevice->Init();
        if (NS_FAILED(rv)) {
            NS_ERROR("nsCacheService::OnProfileChanged: Re-initializing disk device failed");
            gService->mEnableDiskDevice = PR_FALSE;
            // XXX delete mDiskDevice?
        }
    }
    
    if (gService->mMemoryDevice) {
        gService->mMemoryDevice->SetCapacity(gService->mObserver->MemoryCacheCapacity());
        rv = gService->mMemoryDevice->Init();
        if (NS_FAILED(rv) && (rv != NS_ERROR_ALREADY_INITIALIZED)) {
            NS_ERROR("nsCacheService::OnProfileChanged: Re-initializing disk device failed");
            gService->mEnableMemoryDevice = PR_FALSE;
            // XXX delete mMemoryDevice?
        }
    }
}


void
nsCacheService::SetDiskCacheEnabled(PRBool  enabled)
{
    if (!gService)  return;
    nsAutoLock lock(gService->mCacheServiceLock);
    gService->mEnableDiskDevice = enabled;
}


void
nsCacheService::SetDiskCacheCapacity(PRInt32  capacity)
{
    if (!gService)  return;
    nsAutoLock lock(gService->mCacheServiceLock);

    if (gService->mDiskDevice) {
        gService->mDiskDevice->SetCapacity(capacity);
    }
    
    gService->mEnableDiskDevice = gService->mObserver->DiskCacheEnabled();
}


void
nsCacheService::SetMemoryCacheEnabled(PRBool  enabled)
{
    if (!gService)  return;
    nsAutoLock lock(gService->mCacheServiceLock);
    gService->mEnableMemoryDevice = enabled;
    (void) gService->CreateMemoryDevice();    // allocate memory device, if necessary
    
    if (!enabled && gService->mMemoryDevice) {
        // tell memory device to evict everything
        gService->mMemoryDevice->SetCapacity(0);
    }
}


void
nsCacheService::SetMemoryCacheCapacity(PRInt32  capacity)
{
    if (!gService)  return;
    nsAutoLock lock(gService->mCacheServiceLock);
    
    if (gService->mMemoryDevice) {
        gService->mMemoryDevice->SetCapacity(capacity);
    }
    
    gService->mEnableMemoryDevice = gService->mObserver->MemoryCacheEnabled();
}


#if 0
void
nsCacheService::SetCacheDevicesEnabled(PRBool  enableDisk, PRBool  enableMemory)
{
    if (this == nsnull) return;  // NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mCacheServiceLock);
    
    if (enableDisk && !mDiskDevice) {
        // disk device requires lazy activation
    } else if (!enableDisk && mDiskDevice) {
        // XXX deactivate disk
    }

    if (enableMemory && !mMemoryDevice) {
        // XXX enable memory cache device
    } else if (!enableMemory && mMemoryDevice) {
        // XXX disable memory cache device
    }
}
#endif

nsresult
nsCacheService::SetCacheElement(nsCacheEntry * entry, nsISupports * element)
{
    entry->SetThread(PR_GetCurrentThread());
    entry->SetData(element);
    entry->TouchData();
    return NS_OK;
}


nsresult
nsCacheService::OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mCacheServiceLock);

    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here

    return device->OnDataSizeChange(entry, deltaSize);
}


nsresult
nsCacheService::GetTransportForEntry(nsCacheEntry *     entry,
                                     nsCacheAccessMode  mode,
                                     nsITransport    ** result)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mCacheServiceLock);

    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here

    return device->GetTransportForEntry(entry, mode, result);
}


void
nsCacheService::CloseDescriptor(nsCacheEntryDescriptor * descriptor)
{
    NS_ASSERTION(this != nsnull, "CloseDescriptor called with no cache service!");
    if (this == nsnull)  return;
    nsAutoLock lock(mCacheServiceLock);

    // ask entry to remove descriptor
    nsCacheEntry * entry       = descriptor->CacheEntry();
    PRBool         stillActive = entry->RemoveDescriptor(descriptor);
    nsresult       rv          = NS_OK;

    if (!entry->IsValid()) {
        rv = ProcessPendingRequests(entry);
    }

    if (!stillActive) {
        DeactivateEntry(entry);
    }
}


nsresult        
nsCacheService::GetFileForEntry(nsCacheEntry *         entry,
                                nsIFile **             result)
{
    if (this == nsnull)  return NS_ERROR_NOT_AVAILABLE;
    nsAutoLock lock(mCacheServiceLock);
    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here
    return device->GetFileForEntry(entry, result);
}


void
nsCacheService::DeactivateEntry(nsCacheEntry * entry)
{
    nsresult  rv = NS_OK;
    NS_ASSERTION(entry->IsNotInUse(), "### deactivating an entry while in use!");
    nsCacheDevice * device = nsnull;

    if (mMaxDataSize < entry->DataSize() )     mMaxDataSize = entry->DataSize();
    if (mMaxMetaSize < entry->MetaDataSize() ) mMaxMetaSize = entry->MetaDataSize();

    if (entry->IsDoomed()) {
        // remove from Doomed list
        PR_REMOVE_AND_INIT_LINK(entry);
    } else if (entry->IsActive()) {
        // remove from active entries
        mActiveEntries.RemoveEntry(entry);
        entry->MarkInactive();

        // bind entry if necessary to store meta-data
        device = EnsureEntryHasDevice(entry); 
        if (!device) {
            NS_WARNING("DeactivateEntry: unable to bind active entry\n");
            return;
        }
    } else {
        NS_WARNING("DeactivateEntry: bad cache entry state\n");
    }

    device = entry->CacheDevice();
    if (device) {
        rv = device->DeactivateEntry(entry);
        if (NS_FAILED(rv)) {
            // increment deactivate failure count
            ++mDeactivateFailures;
        }
    } else {
        // increment deactivating unbound entry statistic
        ++mDeactivatedUnboundEntries;
        delete entry; // because no one else will
    }
}


nsresult
nsCacheService::ProcessPendingRequests(nsCacheEntry * entry)
{
    nsresult            rv = NS_OK;
    nsCacheRequest *    request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
    nsCacheRequest *    nextRequest;
    PRBool              newWriter = PR_FALSE;
    
    if (request == &entry->mRequestQ)  return NS_OK;    // no queued requests

    if (!entry->IsDoomed() && entry->IsInvalid()) {
        // 1st descriptor closed w/o MarkValid()
        NS_ASSERTION(PR_CLIST_IS_EMPTY(&entry->mDescriptorQ), "shouldn't be here with open descriptors");

#if DEBUG
        // verify no ACCESS_WRITE requests(shouldn't have any of these)
        while (request != &entry->mRequestQ) {
            NS_ASSERTION(request->AccessRequested() != nsICache::ACCESS_WRITE,
                         "ACCESS_WRITE request should have been given a new entry");
            request = (nsCacheRequest *)PR_NEXT_LINK(request);
        }
        request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);        
#endif
        // find first request with ACCESS_READ_WRITE (if any) and promote it to 1st writer
        while (request != &entry->mRequestQ) {
            if (request->AccessRequested() == nsICache::ACCESS_READ_WRITE) {
                newWriter = PR_TRUE;
                break;
            }

            request = (nsCacheRequest *)PR_NEXT_LINK(request);
        }
        
        if (request == &entry->mRequestQ)   // no requests asked for ACCESS_READ_WRITE, back to top
            request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
        
        // XXX what should we do if there are only READ requests in queue?
        // XXX serialize their accesses, give them only read access, but force them to check validate flag?
        // XXX or do readers simply presume the entry is valid
    }

    nsCacheAccessMode  accessGranted = nsICache::ACCESS_NONE;

    while (request != &entry->mRequestQ) {
        nextRequest = (nsCacheRequest *)PR_NEXT_LINK(request);

        if (request->mListener) {

            // Async request
            PR_REMOVE_AND_INIT_LINK(request);

            if (entry->IsDoomed()) {
                rv = ProcessRequest(request, PR_FALSE, nsnull);
                if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)
                    rv = NS_OK;
                else
                    delete request;

                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
            } else if (entry->IsValid() || newWriter) {
                rv = entry->RequestAccess(request, &accessGranted);
                NS_ASSERTION(NS_SUCCEEDED(rv),
                             "if entry is valid, RequestAccess must succeed.");
                // XXX if (newWriter)  NS_ASSERTION( accessGranted == request->AccessRequested(), "why not?");

                // entry->CreateDescriptor dequeues request, and queues descriptor
                nsCOMPtr<nsICacheEntryDescriptor> descriptor;
                rv = entry->CreateDescriptor(request,
                                             accessGranted,
                                             getter_AddRefs(descriptor));
                
                // post call to listener to report error or descriptor
                rv = NotifyListener(request, descriptor, accessGranted, rv);
                delete request;
                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
                
            } else {
                // XXX bad state
            }
        } else {

            // Synchronous request
            request->WakeUp();
        }
        if (newWriter)  break;  // process remaining requests after validation
        request = nextRequest;
    }

    return NS_OK;
}


void
nsCacheService::ClearPendingRequests(nsCacheEntry * entry)
{
    nsCacheRequest * request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
    
    while (request != &entry->mRequestQ) {
        nsCacheRequest * next = (nsCacheRequest *)PR_NEXT_LINK(request);

        // XXX we're just dropping these on the floor for now...definitely wrong.
        PR_REMOVE_AND_INIT_LINK(request);
        delete request;
        request = next;
    }
}


void
nsCacheService::ClearDoomList()
{
    nsCacheEntry * entry = (nsCacheEntry *)PR_LIST_HEAD(&mDoomedEntries);

    while (entry != &mDoomedEntries) {
        nsCacheEntry * next = (nsCacheEntry *)PR_NEXT_LINK(entry);
        
         entry->DetachDescriptors();
         DeactivateEntry(entry);
         entry = next;
    }        
}


void
nsCacheService::ClearActiveEntries()
{
    // XXX really we want a different finalize callback for mActiveEntries
    PL_DHashTableEnumerate(&mActiveEntries.table, DeactivateAndClearEntry, nsnull);
    mActiveEntries.Shutdown();
}


PLDHashOperator PR_CALLBACK
nsCacheService::DeactivateAndClearEntry(PLDHashTable *    table,
                                        PLDHashEntryHdr * hdr,
                                        PRUint32          number,
                                        void *            arg)
{
    nsCacheEntry * entry = ((nsCacheEntryHashTableEntry *)hdr)->cacheEntry;
    NS_ASSERTION(entry, "### active entry = nsnull!");
    gService->ClearPendingRequests(entry);
    entry->DetachDescriptors();
    
    entry->MarkInactive();  // so we don't call Remove() while we're enumerating
    gService->DeactivateEntry(entry);
    
    return PL_DHASH_REMOVE; // and continue enumerating
}


void
nsCacheService::DoomActiveEntries()
{
    nsAutoVoidArray array;

    PL_DHashTableEnumerate(&mActiveEntries.table, RemoveActiveEntry, &array);

    PRUint32 count = array.Count();
    for (PRUint32 i=0; i < count; ++i)
        DoomEntry_Locked((nsCacheEntry *) array[i]);
}


PLDHashOperator PR_CALLBACK
nsCacheService::RemoveActiveEntry(PLDHashTable *    table,
                                  PLDHashEntryHdr * hdr,
                                  PRUint32          number,
                                  void *            arg)
{
    nsCacheEntry * entry = ((nsCacheEntryHashTableEntry *)hdr)->cacheEntry;
    NS_ASSERTION(entry, "### active entry = nsnull!");

    nsVoidArray * array = (nsVoidArray *) arg;
    NS_ASSERTION(array, "### array = nsnull!");
    array->AppendElement(entry);

    // entry is being removed from the active entry list
    entry->MarkInactive();
    return PL_DHASH_REMOVE; // and continue enumerating
}


#if defined(PR_LOGGING)
void
nsCacheService::LogCacheStatistics()
{
    PRUint32 hitPercentage = (PRUint32)((((double)mCacheHits) /
        ((double)(mCacheHits + mCacheMisses))) * 100);
    CACHE_LOG_ALWAYS(("\nCache Service Statistics:\n\n"));
    CACHE_LOG_ALWAYS(("    TotalEntries   = %d\n", mTotalEntries));
    CACHE_LOG_ALWAYS(("    Cache Hits     = %d\n", mCacheHits));
    CACHE_LOG_ALWAYS(("    Cache Misses   = %d\n", mCacheMisses));
    CACHE_LOG_ALWAYS(("    Cache Hit %%    = %d%%\n", hitPercentage));
    CACHE_LOG_ALWAYS(("    Max Key Length = %d\n", mMaxKeyLength));
    CACHE_LOG_ALWAYS(("    Max Meta Size  = %d\n", mMaxMetaSize));
    CACHE_LOG_ALWAYS(("    Max Data Size  = %d\n", mMaxDataSize));
    CACHE_LOG_ALWAYS(("\n"));
    CACHE_LOG_ALWAYS(("    Deactivate Failures         = %d\n", mDeactivateFailures));
    CACHE_LOG_ALWAYS(("    Deactivated Unbound Entries = %d\n", mDeactivatedUnboundEntries));
}
#endif
