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
 * The Original Code is nsDiskCacheDevice.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 */

#include <limits.h>

#include "nsDiskCacheDevice.h"
#include "nsDiskCacheEntry.h"
#include "nsDiskCacheMap.h"
#include "nsDiskCache.h"

#include "nsCacheService.h"
#include "nsCache.h"

#include "nsIFileTransportService.h"
#include "nsITransport.h"
#include "nsICacheVisitor.h"
#include "nsXPIDLString.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISupportsArray.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIObserverService.h"
#include "nsIPref.h"

#include "nsQuickSort.h"

static nsresult ensureCacheDirectory(nsIFile * cacheDirectory);

static const char DISK_CACHE_DEVICE_ID[] = { "disk" };


/******************************************************************************
 *  nsDiskCacheObserver
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark nsDiskCacheObserver
#endif

// Using nsIPref will be subsumed with nsIDirectoryService when a selector
// for the cache directory has been defined.

#define CACHE_DIR_PREF "browser.newcache.directory"
#define CACHE_DISK_CAPACITY_PREF "browser.cache.disk_cache_size"

class nsDiskCacheObserver : public nsIObserver {
    nsDiskCacheDevice* mDevice;
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    nsDiskCacheObserver(nsDiskCacheDevice * device)
        :   mDevice(device)
    {
        NS_INIT_ISUPPORTS();
    }
    
    virtual ~nsDiskCacheObserver() {}
};

NS_IMPL_ISUPPORTS1(nsDiskCacheObserver, nsIObserver);

NS_IMETHODIMP nsDiskCacheObserver::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
    nsresult rv;
    
    // did a preference change?
    if (NS_LITERAL_STRING("nsPref:changed").Equals(aTopic)) {
        nsCOMPtr<nsIPref> prefs = do_QueryInterface(aSubject, &rv);
        if (!prefs) {
            prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;
        }
        
        // which preference changed?
        nsDependentString prefName(someData);
        if (prefName.Equals(NS_LITERAL_STRING(CACHE_DIR_PREF))) {
        	nsCOMPtr<nsILocalFile> cacheDirectory;
            rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs(cacheDirectory));
        	if (NS_SUCCEEDED(rv)) {
                rv = ensureCacheDirectory(cacheDirectory);
                if (NS_SUCCEEDED(rv))
                    mDevice->setCacheDirectory(cacheDirectory);
            }
        } else
        if (prefName.Equals(NS_LITERAL_STRING(CACHE_DISK_CAPACITY_PREF))) {
            PRInt32 cacheCapacity;
            rv = prefs->GetIntPref(CACHE_DISK_CAPACITY_PREF, &cacheCapacity);
        	if (NS_SUCCEEDED(rv))
                mDevice->setCacheCapacity(cacheCapacity * 1024);
        }
    }  else if (NS_LITERAL_STRING("profile-do-change").Equals(aTopic)) {
        // XXX need to regenerate the cache directory. hopefully the
        // cache service has already been informed of this change.
        nsCOMPtr<nsIFile> profileDir;
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, 
                                    getter_AddRefs(profileDir));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsILocalFile> cacheDirectory = do_QueryInterface(profileDir);
            if (cacheDirectory) {
                cacheDirectory->Append("NewCache");
                // always make sure the directory exists, we may have just switched to a new profile dir.
                rv = ensureCacheDirectory(cacheDirectory);
                if (NS_SUCCEEDED(rv))
                    mDevice->setCacheDirectory(cacheDirectory);
            }
        }
    }
    
    return NS_OK;
}

static nsresult ensureCacheDirectory(nsIFile * cacheDirectory)
{
    // make sure the Cache directory exists.
    PRBool exists;
    nsresult rv = cacheDirectory->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = cacheDirectory->Create(nsIFile::DIRECTORY_TYPE, 0777);
    return rv;
}

static nsresult installObservers(nsDiskCacheDevice* device)
{
	nsresult rv;
	nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
	if (NS_FAILED(rv))
		return rv;

    nsCOMPtr<nsIObserver> observer = new nsDiskCacheObserver(device);
    if (!observer) return NS_ERROR_OUT_OF_MEMORY;
    
    device->setPrefsObserver(observer);
    
    rv = prefs->AddObserver(CACHE_DISK_CAPACITY_PREF, observer);
	if (NS_FAILED(rv))
		return rv;

    PRInt32 cacheCapacity;
    rv = prefs->GetIntPref(CACHE_DISK_CAPACITY_PREF, &cacheCapacity);
    if (NS_FAILED(rv)) {
#if DEBUG
        const PRInt32 kTenMegabytes = 10 * 1024 * 1024;
        rv = prefs->SetIntPref(CACHE_DISK_CAPACITY_PREF, kTenMegabytes);
#else
		return rv;
#endif
    } else {
        // XXX note the units of the pref seems to be in kilobytes.
        device->setCacheCapacity(cacheCapacity * 1024);
    }

    rv = prefs->AddObserver(CACHE_DIR_PREF, observer);
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsILocalFile> cacheDirectory;
    rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs(cacheDirectory));
    if (NS_FAILED(rv)) {
        // XXX no preference has been defined, use the directory service. instead.
        nsCOMPtr<nsIFile> profileDir;
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, 
                                    getter_AddRefs(profileDir));
        if (NS_SUCCEEDED(rv)) {
            // XXX this path will probably only succeed when running under Mozilla.
            // store the new disk cache files in a directory called NewCache
            // in the interim.
            cacheDirectory = do_QueryInterface(profileDir, &rv);
        	if (NS_FAILED(rv))
        		return rv;

            rv = cacheDirectory->Append("NewCache");
        	if (NS_FAILED(rv))
        		return rv;

            // XXX since we didn't find a preference, we'll assume that the cache directory
            // can only change when profiles change, so remove the prefs listener and install
            // a profile listener.
            prefs->RemoveObserver(CACHE_DIR_PREF, observer);
            
            nsCOMPtr<nsIObserverService> observerService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
            if (observerService)
                observerService->AddObserver(observer, NS_LITERAL_STRING("profile-do-change").get());
        } else {
#if DEBUG
            // XXX use current process directory during development only.
            nsCOMPtr<nsIFile> currentProcessDir;
            rv = NS_GetSpecialDirectory(NS_XPCOM_CURRENT_PROCESS_DIR, 
                                        getter_AddRefs(currentProcessDir));
        	if (NS_FAILED(rv))
        		return rv;

            cacheDirectory = do_QueryInterface(currentProcessDir, &rv);
        	if (NS_FAILED(rv))
        		return rv;

            rv = cacheDirectory->Append("Cache");
        	if (NS_FAILED(rv))
        		return rv;
#else
            return rv;
#endif
        }
    }

    // always make sure the directory exists, the user may blow it away.
    rv = ensureCacheDirectory(cacheDirectory);
    if (NS_FAILED(rv))
        return rv;

    // cause the preference to be set up initially.
    device->setCacheDirectory(cacheDirectory);
    
    return NS_OK;
}

static nsresult removeObservers(nsDiskCacheDevice* device)
{
    nsresult rv;

    nsCOMPtr<nsIObserver> observer;
    device->getPrefsObserver(getter_AddRefs(observer));
    device->setPrefsObserver(nsnull);
    NS_ASSERTION(observer, "removePrefListeners");

    if (observer) {
    	nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
    	if (prefs) {
            prefs->RemoveObserver(CACHE_DISK_CAPACITY_PREF, observer);
            prefs->RemoveObserver(CACHE_DIR_PREF, observer);
        }
        
        nsCOMPtr<nsIObserverService> observerService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (observerService)
            observerService->RemoveObserver(observer, NS_LITERAL_STRING("profile-do-change").get());
    }
    
    return NS_OK;
}


/******************************************************************************
 *  nsDiskCacheEvictor
 *
 *  Helper class for nsDiskCacheDevice.
 *
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark -
#pragma mark nsDiskCacheEvictor
#endif

class nsDiskCacheEvictor : public nsDiskCacheRecordVisitor
{
public:
    nsDiskCacheEvictor( nsDiskCacheDevice *   device,
                        nsDiskCacheMap *      cacheMap,
                        nsDiskCacheBindery *  cacheBindery,
                        PRInt32               targetSize,
                        const char *          clientID)
        : mDevice(device)
        , mCacheMap(cacheMap)
        , mBindery(cacheBindery)
        , mTargetSize(targetSize)
        , mClientID(clientID)
    {}
    
    virtual PRInt32  VisitRecord(nsDiskCacheRecord *  mapRecord);
 
private:
        nsDiskCacheDevice *  mDevice;
        nsDiskCacheMap *     mCacheMap;
        nsDiskCacheBindery * mBindery;
        PRInt32              mTargetSize;
        const char *         mClientID;
};


PRInt32
nsDiskCacheEvictor::VisitRecord(nsDiskCacheRecord *  mapRecord)
{
    // read disk cache entry
    nsDiskCacheEntry *   diskEntry = nsnull;
    nsDiskCacheBinding * binding   = nsnull;
    char *               clientID  = nsnull;
    PRInt32              result    = kVisitNextRecord;

    if (mClientID) {
         // we're just evicting records for a specific client
        nsresult  rv = mCacheMap->ReadDiskCacheEntry(mapRecord, &diskEntry);
        if (NS_FAILED(rv))  goto exit;  // XXX or delete record?
    
        // XXX FIXME compare clientID's without malloc

        // get client ID from key
        rv = ClientIDFromCacheKey(nsDependentCString(diskEntry->mKeyStart), &clientID);
        if (NS_FAILED(rv))  goto exit;
         
        if (nsCRT::strcmp(mClientID, clientID) != 0) goto exit;
    }
    
    if (mCacheMap->TotalSize() < mTargetSize) {
        result = kStopVisitingRecords;
        goto exit;
    }
    
    binding = mBindery->FindActiveBinding(mapRecord->HashNumber());
    if (binding) {
        // we are currently using this entry, so all we can do is doom it
        
        // since we're enumerating the records, we don't want to call DeleteRecord
        // when nsCacheService::GlobalInstance()->DoomEntry_Locked() calls us back.
        binding->mDoomed = PR_TRUE;         // mark binding record as 'deleted'
        nsCacheService::GlobalInstance()->DoomEntry_Locked(binding->mCacheEntry);
        result = kDeleteRecordAndContinue;  // this will REALLY delete the record

    } else {
        // entry not in use, just delete storage because we're enumerating the records
        (void) mCacheMap->DeleteStorage(mapRecord);
        result = kDeleteRecordAndContinue;  // this will REALLY delete the record
    }

exit:
    
    delete clientID;
    delete diskEntry;
    return result;
}


/******************************************************************************
 *  nsDiskCacheDeviceInfo
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark -
#pragma mark nsDiskCacheDeviceInfo
#endif

class nsDiskCacheDeviceInfo : public nsICacheDeviceInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEDEVICEINFO

    nsDiskCacheDeviceInfo(nsDiskCacheDevice* device)
        :   mDevice(device)
    {
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsDiskCacheDeviceInfo() {}
    
private:
    nsDiskCacheDevice* mDevice;
};

NS_IMPL_ISUPPORTS1(nsDiskCacheDeviceInfo, nsICacheDeviceInfo);

/* readonly attribute string description; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetDescription(char ** aDescription)
{
    NS_ENSURE_ARG_POINTER(aDescription);
    *aDescription = nsCRT::strdup("Disk cache device");
    return *aDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute string usageReport; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetUsageReport(char ** usageReport)
{
    NS_ENSURE_ARG_POINTER(usageReport);
    nsCString buffer;
    
    buffer.Assign("<table>\n");

    buffer.Append("<tr><td><b>Cache Directory:</b></td><td><tt> ");
    nsCOMPtr<nsILocalFile> cacheDir;
    char *                 path;
    mDevice->getCacheDirectory(getter_AddRefs(cacheDir)); 
    nsresult rv = cacheDir->GetPath(&path);
    if (NS_SUCCEEDED(rv)) {
        buffer.Append(path);
    } else {
        buffer.Append("directory unavailable");
    }
    buffer.Append("</tt></td></tr>");
    // buffer.Append("<tr><td><b>Files:</b></td><td><tt> XXX</tt></td></tr>");
    buffer.Append("</table>");
    *usageReport = buffer.ToNewCString();
    if (!*usageReport) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

/* readonly attribute unsigned long entryCount; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetEntryCount(PRUint32 *aEntryCount)
{
    NS_ENSURE_ARG_POINTER(aEntryCount);
    *aEntryCount = mDevice->getEntryCount();
    return NS_OK;
}

/* readonly attribute unsigned long totalSize; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetTotalSize(PRUint32 *aTotalSize)
{
    NS_ENSURE_ARG_POINTER(aTotalSize);
    *aTotalSize = mDevice->getCacheSize();
    return NS_OK;
}

/* readonly attribute unsigned long maximumSize; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetMaximumSize(PRUint32 *aMaximumSize)
{
    NS_ENSURE_ARG_POINTER(aMaximumSize);
    *aMaximumSize = mDevice->getCacheCapacity();
    return NS_OK;
}


/******************************************************************************
 *  nsDiskCache
 *****************************************************************************/
#ifdef XP_MAC
#pragma mark -
#pragma mark nsDiskCache
#endif

/**
 *  nsDiskCache::Hash(const char * key)
 *
 *  This algorithm of this method implies nsDiskCacheRecords will be stored
 *  in a certain order on disk.  If the algorithm changes, existing cache
 *  map files may become invalid, and therefore the kCurrentVersion needs
 *  to be revised.
 */
PLDHashNumber
nsDiskCache::Hash(const char * key)
{
    PLDHashNumber h = 0;
    for (const PRUint8* s = (PRUint8*) key; *s != '\0'; ++s)
        h = (h >> (PL_DHASH_BITS - 4)) ^ (h << 4) ^ *s;
    return (h == 0 ? ULONG_MAX : h);
}


/******************************************************************************
 *  nsDiskCacheDevice
 *****************************************************************************/
static nsCOMPtr<nsIFileTransportService> gFileTransportService;

#ifdef XP_MAC
#pragma mark -
#pragma mark nsDiskCacheDevice
#endif

nsDiskCacheDevice::nsDiskCacheDevice()
    :   mInitialized(PR_FALSE), mCacheCapacity(0), mCacheMap(nsnull)
{
}

nsDiskCacheDevice::~nsDiskCacheDevice()
{
    Shutdown();
    delete mCacheMap;
}


/**
 *  methods of nsCacheDevice
 */
nsresult
nsDiskCacheDevice::Init()
{
    nsresult rv;
    
    rv = mBindery.Init();
    if (NS_FAILED(rv)) return rv;

    // XXX examine preferences, and install observers to react to changes.
    rv = installObservers(this);
    if (NS_FAILED(rv)) return rv;
    
    // hold the file transport service to avoid excessive calls to the service manager.
    gFileTransportService = do_GetService("@mozilla.org/network/file-transport-service;1", &rv);
    if (NS_FAILED(rv)) return rv;

    // XXX are we sure we want to do this on startup?
    // delete "Cache.Trash" folder
    nsCOMPtr<nsIFile> cacheTrashDir;
    rv = GetCacheTrashDirectory(getter_AddRefs(cacheTrashDir));
    if (NS_FAILED(rv))  goto error_exit;
    (void) cacheTrashDir->Delete(PR_TRUE);      // ignore errors, we tried...

    // Try opening cache map file.
    mCacheMap = new nsDiskCacheMap;
    if (!mCacheMap) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error_exit;
    }

    rv = mCacheMap->Open(mCacheDirectory);
    if (NS_FAILED(rv)) {
        rv = InitializeCacheDirectory();        // retry one time
        if (NS_FAILED(rv))  goto error_exit;
    }
    
    mInitialized = PR_TRUE;                     // record that initialization succeeded.
    return NS_OK;

error_exit:
    // XXX de-install observers?
    if (mCacheMap)  {
        (void) mCacheMap->Close();
        delete mCacheMap;
        mCacheMap = nsnull;
    }
    gFileTransportService   = nsnull;

    return rv;
}


nsresult
nsDiskCacheDevice::Shutdown()
{
    if (mInitialized) {
        // check cache limits in case we need to evict.
        EvictDiskCacheEntries();

        // write out persistent information about the cache.
        (void) mCacheMap->Close();

        // no longer initialized.
        mInitialized = PR_FALSE;
    }

    // disconnect observers.
    removeObservers(this);
    
    // release the reference to the cached file transport service.
    gFileTransportService = nsnull;
    
    return NS_OK;
}


nsresult
nsDiskCacheDevice::Create(nsCacheDevice **result)
{
    nsDiskCacheDevice * device = new nsDiskCacheDevice();
    if (!device)  return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = device->Init();
    if (NS_FAILED(rv)) {
        delete device;
        device = nsnull;
    }
    *result = device;
    return rv;
}


const char *
nsDiskCacheDevice::GetDeviceID()
{
    return DISK_CACHE_DEVICE_ID;
}


/**
 *  FindEntry -
 *
 *      cases:  key not in disk cache, hash number free
 *              key not in disk cache, hash number used
 *              key in disk cache
 */
nsCacheEntry *
nsDiskCacheDevice::FindEntry(nsCString * key)
{
    nsresult                rv;
    nsDiskCacheRecord       record;
    nsCacheEntry *          entry   = nsnull;
    nsDiskCacheBinding *    binding = nsnull;
    PLDHashNumber           hashNumber = nsDiskCache::Hash(key->get());

#if DEBUG  /*because we shouldn't be called for active entries */
    binding = mBindery.FindActiveBinding(hashNumber);
    NS_ASSERTION(!binding, "### FindEntry() called for a bound entry.");
    binding = nsnull;
#endif
    
    // lookup hash number in cache map
    rv = mCacheMap->FindRecord(hashNumber, &record);
    if (NS_FAILED(rv))  return nsnull;  // XXX log error?
    
    nsDiskCacheEntry * diskEntry;
    rv = mCacheMap->ReadDiskCacheEntry(&record, &diskEntry);
    if (NS_FAILED(rv))  return nsnull;
    
    // compare key to be sure
    if (nsCRT::strcmp(diskEntry->mKeyStart, key->get()) == 0) {
        entry = diskEntry->CreateCacheEntry(this);
    }
    delete diskEntry;
    
    // If we had a hash collision or CreateCacheEntry failed, return nsnull
    if (!entry)  return nsnull;
    
    binding = mBindery.CreateBinding(entry, &record);
    if (!binding) {
        delete entry;
        return nsnull;
    }
    
    return entry;
}


nsresult
nsDiskCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    nsresult              rv = NS_OK;
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    NS_ASSERTION(binding, "DeactivateEntry: binding == nsnull");
    if (!binding)  return NS_ERROR_UNEXPECTED;

    if (entry->IsDoomed()) {
        // delete data, entry, record from disk for entry
        rv = mCacheMap->DeleteStorage(&binding->mRecord);

    } else {
        // save stuff to disk for entry
        rv = mCacheMap->WriteDiskCacheEntry(binding);
        if (NS_FAILED(rv)) {
            // clean up as best we can
            (void) mCacheMap->DeleteRecordAndStorage(&binding->mRecord);
            binding->mDoomed = PR_TRUE; // record is no longer in cache map
        }
    }

    mBindery.RemoveBinding(binding); // extract binding from collision detection stuff
    delete entry;   // which will release binding
    return rv;
}


/**
 * BindEntry()
 *      no hash number collision -> no problem
 *      collision
 *          record not active -> evict, no problem
 *          record is active
 *              record is already doomed -> record shouldn't have been in map, no problem
 *              record is not doomed -> doom, and replace record in map
 *              
 *              walk matching hashnumber list to find lowest generation number
 *              take generation number from other (data/meta) location,
 *                  or walk active list
 */
nsresult
nsDiskCacheDevice::BindEntry(nsCacheEntry * entry)
{
    nsresult rv = NS_OK;
    nsDiskCacheRecord record, oldRecord;
    
    // create a new record for this entry
    record.SetHashNumber(nsDiskCache::Hash(entry->Key()->get()));
    record.SetEvictionRank(ULONG_MAX - SecondsFromPRTime(PR_Now()));

    if (!entry->IsDoomed()) {
        // if entry isn't doomed, add it to the cache map
        rv = mCacheMap->AddRecord(&record, &oldRecord); // deletes old record, if any
        if (NS_FAILED(rv))  return rv;
        
        PRUint32    oldHashNumber = oldRecord.HashNumber();
        if (oldHashNumber) {
            // gotta evict this one first
            nsDiskCacheBinding * oldBinding = mBindery.FindActiveBinding(oldHashNumber);
            if (oldBinding) {
                // XXX if debug : compare keys for hashNumber collision

                if (!oldBinding->mCacheEntry->IsDoomed()) {
                // we've got a live one!
                    nsCacheService::GlobalInstance()->DoomEntry_Locked(oldBinding->mCacheEntry);
                    // storage will be delete when oldBinding->mCacheEntry is Deactivated
                }
            } else {
                // delete storage
                // XXX if debug : compare keys for hashNumber collision
                rv = mCacheMap->DeleteStorage(&oldRecord);
                if (NS_FAILED(rv))  return rv;  // XXX delete record we just added?
            }
        }
    }
    
    // Make sure this entry has its associated nsDiskCacheBinding attached.
    nsDiskCacheBinding *  binding = mBindery.CreateBinding(entry, &record);
    NS_ASSERTION(binding, "nsDiskCacheDevice::BindEntry");
    if (!binding) return NS_ERROR_OUT_OF_MEMORY;
    NS_ASSERTION(binding->mRecord.ValidRecord(), "bad cache map record");


#if 0
    // XXX from old code: when would we bind an entry that already has data?
    PRUint32 dataSize = newEntry->DataSize();
    if (dataSize)
        OnDataSizeChange(newEntry, dataSize);
#endif

    return NS_OK;
}


void
nsDiskCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    NS_ASSERTION(binding, "DoomEntry: binding == nsnull");
    if (!binding)  return;

    if (!binding->mDoomed) {
        // so it can't be seen by FindEntry() ever again.
        nsresult rv = mCacheMap->DoomRecord(&binding->mRecord);
        NS_ASSERTION(NS_SUCCEEDED(rv), "DoomRecord failed.");
        binding->mDoomed = PR_TRUE; // record in no longer in cache map
    }
}


nsresult
nsDiskCacheDevice::GetTransportForEntry(nsCacheEntry * entry,
                                        nsCacheAccessMode mode, 
                                        nsITransport ** result)
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    nsresult             rv;
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    NS_ASSERTION(binding, "GetTransportForEntry: binding == nsnull");
    if (!binding)  return NS_ERROR_UNEXPECTED;
    
    NS_ASSERTION(binding->mCacheEntry == entry, "binding & entry don't point to each other");

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    nsCOMPtr<nsITransport>& transport = binding->getTransport(mode);
    if (transport) {
        NS_ADDREF(*result = transport);
        return NS_OK;
    }
#endif

    // check/set binding->mRecord for separate file, sync w/mCacheMap
    if (binding->mRecord.DataLocationInitialized()) {
        NS_ASSERTION(binding->mRecord.DataFile() == 0, "error: cache block file"); // make sure it's a separate file
        NS_ASSERTION(binding->mRecord.DataFileGeneration() == binding->mGeneration, "error generations out of sync");
    } else {
        binding->mRecord.SetDataFileGeneration(binding->mGeneration);
        binding->mRecord.SetDataFileSize(0);    // 1k minimum
        if (!binding->mDoomed) {
            // record stored in cache map, so update it
            rv = mCacheMap->UpdateRecord(&binding->mRecord);
            if (NS_FAILED(rv))  return rv;
        }
    }

    // generate the name of the cache entry from the hash code of its key,
    // modulo the number of files we're willing to keep cached.
    nsCOMPtr<nsIFile> file;
    rv = mCacheMap->GetFileForDiskCacheRecord(&binding->mRecord,
                                              nsDiskCache::kData,
                                              getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;
    
    PRInt32 ioFlags = 0;
    switch (mode) {
    case nsICache::ACCESS_READ:
        ioFlags = PR_RDONLY;
        break;
    case nsICache::ACCESS_WRITE:
        ioFlags = PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE;
        break;
    case nsICache::ACCESS_READ_WRITE:
        ioFlags = PR_RDWR | PR_CREATE_FILE;
        break;
    }

    rv = gFileTransportService->CreateTransport(file, ioFlags, PR_IRUSR | PR_IWUSR, result);
    return rv;
}


nsresult
nsDiskCacheDevice::GetFileForEntry(nsCacheEntry *    entry,
                                   nsIFile **        result)
{
    nsresult             rv;
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    NS_ASSERTION(binding, "GetFileForEntry: binding == nsnull");
    if (!binding)  return NS_ERROR_UNEXPECTED;
    
    // check/set binding->mRecord for separate file, sync w/mCacheMap
    if (binding->mRecord.DataLocationInitialized()) {
        NS_ASSERTION(binding->mRecord.DataFile() == 0, "error: cache block file"); // make sure it's a separate file
        NS_ASSERTION(binding->mRecord.DataFileGeneration() == binding->mGeneration, "error generations out of sync");
    } else {
        binding->mRecord.SetDataFileGeneration(binding->mGeneration);
        binding->mRecord.SetDataFileSize(0);    // 1k minimum
        if (!binding->mDoomed) {
            // record stored in cache map, so update it
            rv = mCacheMap->UpdateRecord(&binding->mRecord);
            if (NS_FAILED(rv))  return rv;
        }
    }
    
    nsCOMPtr<nsIFile>  file;
    rv = mCacheMap->GetFileForDiskCacheRecord(&binding->mRecord,
                                              nsDiskCache::kData,
                                              getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;
    
    NS_IF_ADDREF(*result = file);
    return NS_OK;
}


/**
 * This routine will get called every time an open descriptor is written to.
 */
nsresult
nsDiskCacheDevice::OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize)
{
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    NS_ASSERTION(binding, "OnDataSizeChange: binding == nsnull");
    if (!binding)  return NS_ERROR_UNEXPECTED;

    NS_ASSERTION(binding->mRecord.ValidRecord(), "bad record");

    PRUint32  sizeK = ((entry->DataSize() + 0x0399) >> 10); // round up to next 1k
    PRUint32  newSizeK =  ((entry->DataSize() + deltaSize + 0x399) >> 10);

    NS_ASSERTION((sizeK < USHRT_MAX) && (sizeK == binding->mRecord.DataFileSize()),
                 "data size out of sync");

    mCacheMap->IncrementTotalSize((newSizeK - sizeK) * 1024);
    newSizeK = (newSizeK < USHRT_MAX) ? newSizeK : USHRT_MAX;   // record file size ceiling
    binding->mRecord.SetDataFileSize(newSizeK);     // update binding->mRecord

    EvictDiskCacheEntries();
    
    return NS_OK;
}


/******************************************************************************
 *  EntryInfoVisitor
 *****************************************************************************/
class EntryInfoVisitor : public nsDiskCacheRecordVisitor
{
public:
    EntryInfoVisitor(nsDiskCacheDevice * device,
                     nsDiskCacheMap *    cacheMap,
                     nsICacheVisitor *   visitor)
        : mDevice(device)
        , mCacheMap(cacheMap)
        , mVisitor(visitor)
        , mResult(NS_OK)
    {}
    
    virtual PRInt32  VisitRecord(nsDiskCacheRecord *  mapRecord)
    {
        // XXX optimization: do we have this record in memory?
        
        // read in the entry (metadata)
        nsDiskCacheEntry * diskEntry;
        nsresult rv = mCacheMap->ReadDiskCacheEntry(mapRecord, &diskEntry);
        if (NS_FAILED(rv)) {
            mResult = rv;
            return kStopVisitingRecords;
        }

        // create nsICacheEntryInfo
        nsDiskCacheEntryInfo * entryInfo = new nsDiskCacheEntryInfo(DISK_CACHE_DEVICE_ID, diskEntry);
        if (!entryInfo) {
            mResult = NS_ERROR_OUT_OF_MEMORY;
            return kStopVisitingRecords;
        }
        nsCOMPtr<nsICacheEntryInfo> ref(entryInfo);
        
        PRBool  keepGoing;
        rv = mVisitor->VisitEntry(DISK_CACHE_DEVICE_ID, entryInfo, &keepGoing);
        delete diskEntry;
        return keepGoing ? kVisitNextRecord : kStopVisitingRecords;
    }
 
private:
        nsDiskCacheDevice * mDevice;
        nsDiskCacheMap *    mCacheMap;
        nsICacheVisitor *   mVisitor;
        nsresult            mResult;
};


nsresult
nsDiskCacheDevice::Visit(nsICacheVisitor * visitor)
{
    nsDiskCacheDeviceInfo* deviceInfo = new nsDiskCacheDeviceInfo(this);
    nsCOMPtr<nsICacheDeviceInfo> ref(deviceInfo);
    
    PRBool keepGoing;
    nsresult rv = visitor->VisitDevice(DISK_CACHE_DEVICE_ID, deviceInfo, &keepGoing);
    if (NS_FAILED(rv)) return rv;
    
    if (keepGoing) {
        EntryInfoVisitor  infoVisitor(this, mCacheMap, visitor);
        return mCacheMap->VisitRecords(&infoVisitor);
    }

    return NS_OK;
}


nsresult
nsDiskCacheDevice::EvictEntries(const char * clientID)
{
    nsDiskCacheEvictor  evictor(this, mCacheMap, &mBindery, 0, clientID);
    nsresult       rv = mCacheMap->VisitRecords(&evictor);
    return rv;
}


/**
 *  private methods
 */
#ifdef XP_MAC
#pragma mark -
#pragma mark PRIVATE METHODS
#endif

nsresult
nsDiskCacheDevice::InitializeCacheDirectory()
{
    nsresult rv;
    
    // recursively delete the disk cache directory.
    rv = mCacheDirectory->Delete(PR_TRUE);
    if (NS_FAILED(rv)) {
        // try moving it aside
        
        // create "Cache.Trash" directory if necessary
        nsCOMPtr<nsIFile> cacheTrashDir;
        rv = GetCacheTrashDirectory(getter_AddRefs(cacheTrashDir));
        if (NS_FAILED(rv))  return rv;
        
        PRBool exists = PR_FALSE;
        rv = cacheTrashDir->Exists(&exists);
        if (NS_FAILED(rv))  return rv;
        
        if (!exists) {
            // create the "Cache.Trash" directory
            rv = cacheTrashDir->Create(nsIFile::DIRECTORY_TYPE,0777);
            if (NS_FAILED(rv))  return rv;
        }
        
        // create a directory with unique name to contain existing cache directory
        rv = cacheTrashDir->Append("Cache");
        if (NS_FAILED(rv))  return rv;
        rv = cacheTrashDir->CreateUnique(nsnull,nsIFile::DIRECTORY_TYPE, 0777); 
        if (NS_FAILED(rv))  return rv;
        
        // move existing cache directory into profileDir/Cache.Trash/CacheUnique
        nsCOMPtr<nsIFile> existingCacheDir;
        rv = mCacheDirectory->Clone(getter_AddRefs(existingCacheDir));
        if (NS_FAILED(rv))  return rv;
        rv = existingCacheDir->MoveTo(cacheTrashDir, nsnull);
        if (NS_FAILED(rv))  return rv;
    }
    
    rv = mCacheDirectory->Create(nsIFile::DIRECTORY_TYPE, 0777);
    if (NS_FAILED(rv)) return rv;
    
    // reopen the cache map     
    rv = mCacheMap->Open(mCacheDirectory);
    return rv;
}


nsresult
nsDiskCacheDevice::GetCacheTrashDirectory(nsIFile ** result)
{
    nsCOMPtr<nsIFile> cacheTrashDir;
    nsresult rv = mCacheDirectory->Clone(getter_AddRefs(cacheTrashDir));
    if (NS_FAILED(rv))  return rv;
    rv = cacheTrashDir->SetLeafName("Cache.Trash");
    if (NS_FAILED(rv))  return rv;
    
    *result = cacheTrashDir.get();
    NS_ADDREF(*result);
    return rv;
}


nsresult
nsDiskCacheDevice::EvictDiskCacheEntries()
{
    nsresult rv;
    
    if (mCacheMap->TotalSize() < mCacheCapacity)  return NS_OK;

    nsDiskCacheEvictor  evictor(this, mCacheMap, &mBindery, mCacheCapacity, nsnull);
    rv = mCacheMap->EvictRecords(&evictor);
    
    return rv;
}


/**
 *  methods for prefs
 */
#ifdef XP_MAC
#pragma mark -
#pragma mark PREF METHODS
#endif
void nsDiskCacheDevice::setPrefsObserver(nsIObserver* observer)
{
    mPrefsObserver = observer;
}

void nsDiskCacheDevice::getPrefsObserver(nsIObserver** result)
{
    NS_IF_ADDREF(*result = mPrefsObserver);
}

void nsDiskCacheDevice::setCacheDirectory(nsILocalFile* cacheDirectory)
{
    mCacheDirectory = cacheDirectory;
}


void
nsDiskCacheDevice::getCacheDirectory(nsILocalFile ** result)
{
    *result = mCacheDirectory;
    NS_IF_ADDREF(*result);
}


void nsDiskCacheDevice::setCacheCapacity(PRUint32 capacity)
{
    mCacheCapacity = capacity;
    if (mInitialized) {
        // start evicting entries if the new size is smaller!
        // XXX need to enter cache service lock here!
        EvictDiskCacheEntries();
    }
}

PRUint32 nsDiskCacheDevice::getCacheCapacity()
{
    return mCacheCapacity;
}

PRUint32 nsDiskCacheDevice::getCacheSize()
{
    return mCacheMap->TotalSize();
}

PRUint32 nsDiskCacheDevice::getEntryCount()
{
    return mCacheMap->EntryCount();
}
