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
 * The Original Code is nsMemoryCacheDevice.cpp, released February 22, 2001.
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

#include "nsDiskCacheDevice.h"
#include "nsDiskCacheEntry.h"
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

static const char DISK_CACHE_DEVICE_ID[] = { "disk" };

static FILE* openFileStream(nsIFile* file, const char* mode);

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

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
    nsLiteralString aTopicString(aTopic);
    if (aTopicString.Equals(NS_LITERAL_STRING("nsPref:changed"))) {
        // when bug #71879 gets fixed, this QueryInterface will succeed!
        nsCOMPtr<nsIPref> prefs = do_QueryInterface(aSubject, &rv);
        if (!prefs) {
            prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;
        }
        
        // which preference changed?
        nsLiteralString prefName(someData);
        if (prefName.Equals(NS_LITERAL_STRING(CACHE_DIR_PREF))) {
        	nsCOMPtr<nsILocalFile> cacheDirectory;
            rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs(cacheDirectory));
        	if (NS_SUCCEEDED(rv))
                mDevice->setCacheDirectory(cacheDirectory);
        } else
        if (prefName.Equals(NS_LITERAL_STRING(CACHE_DISK_CAPACITY_PREF))) {
            PRInt32 cacheCapacity;
            rv = prefs->GetIntPref(CACHE_DISK_CAPACITY_PREF, &cacheCapacity);
        	if (NS_SUCCEEDED(rv))
                mDevice->setCacheCapacity(cacheCapacity);
        }
    }  else if (aTopicString.Equals(NS_LITERAL_STRING("profile-do-change"))) {
        // XXX need to regenerate the cache directory. hopefully the
        // cache service has already been informed of this change.
        nsCOMPtr<nsIFile> profileDir;
        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, 
                                    getter_AddRefs(profileDir));
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsILocalFile> cacheDirectory = do_QueryInterface(profileDir);
            if (cacheDirectory) {
                cacheDirectory->Append("NewCache");
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

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

static nsDiskCacheEntry*
ensureDiskCacheEntry(nsCacheEntry * entry)
{
    nsCOMPtr<nsISupports> data;
    nsresult rv = entry->GetData(getter_AddRefs(data));
    if (NS_SUCCEEDED(rv) && !data) {
        nsDiskCacheEntry* diskEntry = new nsDiskCacheEntry(entry);
        data = diskEntry;
        if (NS_SUCCEEDED(rv) && data)
            entry->SetData(data.get());
    }
    return (nsDiskCacheEntry*) (nsISupports*) data.get();
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

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
    char* result = nsCRT::strdup("disk cache device");
    if (!result) return NS_ERROR_OUT_OF_MEMORY;
    *aDescription = result;
    return NS_OK;
}

/* readonly attribute string usageReport; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetUsageReport(char ** aUsageReport)
{
    char* result = nsCRT::strdup("disk cache usage report");
    if (!result) return NS_ERROR_OUT_OF_MEMORY;
    *aUsageReport = result;
    return NS_OK;
}

/* readonly attribute unsigned long entryCount; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetEntryCount(PRUint32 *aEntryCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute unsigned long totalSize; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetTotalSize(PRUint32 *aTotalSize)
{
    *aTotalSize = mDevice->getCacheSize();
    return NS_OK;
}

/* readonly attribute unsigned long maximumSize; */
NS_IMETHODIMP nsDiskCacheDeviceInfo::GetMaximumSize(PRUint32 *aMaximumSize)
{
    *aMaximumSize = mDevice->getCacheCapacity();
    return NS_OK;
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

struct MetaDataHeader {
    PRUint32        mHeaderSize;
    PRInt32         mFetchCount;
    PRUint32        mLastFetched;
    PRUint32        mLastModified;
    PRUint32        mExpirationTime;
    PRUint32        mDataSize;
    PRUint32        mKeySize;
    PRUint32        mMetaDataSize;
    // followed by null-terminated key and metadata string values.

    MetaDataHeader()
        :   mHeaderSize(sizeof(MetaDataHeader)),
            mFetchCount(0),
            mLastFetched(0),
            mLastModified(0),
            mExpirationTime(0),
            mDataSize(0),
            mKeySize(0),
            mMetaDataSize(0)
    {
    }

    MetaDataHeader(nsCacheEntry* entry)
        :   mHeaderSize(sizeof(MetaDataHeader)),
            mFetchCount(entry->FetchCount()),
            mLastFetched(entry->LastFetched()),
            mLastModified(entry->LastModified()),
            mExpirationTime(entry->ExpirationTime()),
            mDataSize(entry->DataSize()),
            mKeySize(entry->Key()->Length() + 1),
            mMetaDataSize(0)
    {
    }
};

struct MetaDataFile : MetaDataHeader {
    char*           mKey;
    char*           mMetaData;

    MetaDataFile()
        :   mKey(nsnull), mMetaData(nsnull)
    {
    }
    
    MetaDataFile(nsCacheEntry* entry)
        :   MetaDataHeader(entry),
            mKey(nsnull), mMetaData(nsnull)
    {
    }
    
    ~MetaDataFile()
    {
        delete[] mKey;
        delete[] mMetaData;
    }
    
    nsresult Init(nsCacheEntry* entry)
    {
        PRUint32 size = 1 + entry->Key()->Length();
        mKey = new char[size];
        if (!mKey) return NS_ERROR_OUT_OF_MEMORY;
        nsCRT::memcpy(mKey, entry->Key()->get(), size);
        return entry->FlattenMetaData(&mMetaData, &mMetaDataSize);
    }

    nsresult Write(nsIOutputStream* output);
    nsresult Read(nsIInputStream* input);
};

nsresult MetaDataFile::Write(nsIOutputStream* output)
{
    nsresult rv;
    PRUint32 count;
    
    rv = output->Write((char*)this, mHeaderSize, &count);
    if (NS_FAILED(rv)) return rv;
    
    // write the key to the file.
    rv = output->Write(mKey, mKeySize, &count);
    if (NS_FAILED(rv)) return rv;
    
    // write the flattened metadata to the file.
    if (mMetaDataSize) {
        rv = output->Write(mMetaData, mMetaDataSize, &count);
        if (NS_FAILED(rv)) return rv;
    }
    
    return NS_OK;
}

nsresult MetaDataFile::Read(nsIInputStream* input)
{
    nsresult rv;
    PRUint32 count;
    
    // read the header size used by this file.
    rv = input->Read((char*)&mHeaderSize, sizeof(mHeaderSize), &count);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(mHeaderSize == sizeof(MetaDataHeader), "### CACHE FORMAT CHANGED!!! PLEASE DELETE YOUR CACHE DIRECTORY!!! ###");
    rv = input->Read((char*)&mFetchCount, mHeaderSize - sizeof(mHeaderSize), &count);
    if (NS_FAILED(rv)) return rv;

    // read in the key.
    delete[] mKey;
    mKey = new char[mKeySize];
    if (!mKey) return NS_ERROR_OUT_OF_MEMORY;
    rv = input->Read(mKey, mKeySize, &count);
    if (NS_FAILED(rv)) return rv;

    // read in the metadata.
    delete mMetaData;
    mMetaData = nsnull;
    if (mMetaDataSize) {
        mMetaData = new char[mMetaDataSize];
        if (!mMetaData) return NS_ERROR_OUT_OF_MEMORY;
        rv = input->Read(mMetaData, mMetaDataSize, &count);
        if (NS_FAILED(rv)) return rv;
    }
    
    return NS_OK;
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

class nsDiskCacheEntryInfo : public nsICacheEntryInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYINFO

    nsDiskCacheEntryInfo()
    {
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsDiskCacheEntryInfo() {}
    
    nsresult Read(nsIInputStream * input)
    {
        return mMetaDataFile.Read(input);
    }
    
    const char* Key() { return mMetaDataFile.mKey; }
    
private:
    MetaDataFile mMetaDataFile;
};
NS_IMPL_ISUPPORTS1(nsDiskCacheEntryInfo, nsICacheEntryInfo);

NS_IMETHODIMP nsDiskCacheEntryInfo::GetClientID(char ** clientID)
{
    NS_ENSURE_ARG_POINTER(clientID);
    return ClientIDFromCacheKey(nsLiteralCString(mMetaDataFile.mKey), clientID);
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetKey(char ** clientKey)
{
    NS_ENSURE_ARG_POINTER(clientKey);
    return ClientKeyFromCacheKey(nsLiteralCString(mMetaDataFile.mKey), clientKey);
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetFetchCount(PRInt32 *aFetchCount)
{
    return *aFetchCount = mMetaDataFile.mFetchCount;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetLastFetched(PRUint32 *aLastFetched)
{
    *aLastFetched = mMetaDataFile.mLastFetched;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetLastModified(PRUint32 *aLastModified)
{
    *aLastModified = mMetaDataFile.mLastModified;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetLastValidated(PRUint32 *aLastValidated)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetExpirationTime(PRUint32 *aExpirationTime)
{
    *aExpirationTime = mMetaDataFile.mExpirationTime;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::IsStreamBased(PRBool *aStreamBased)
{
    *aStreamBased = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheEntryInfo::GetDataSize(PRUint32 *aDataSize)
{
    *aDataSize = mMetaDataFile.mDataSize;
    return NS_OK;
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

static nsCOMPtr<nsIFileTransportService> gFileTransportService;

nsDiskCacheDevice::nsDiskCacheDevice()
    :   mCacheCapacity(0), mCacheSize(0)
{
}

nsDiskCacheDevice::~nsDiskCacheDevice()
{
    removeObservers(this);

#if 1
    // XXX implement poor man's eviction strategy right here,
    // keep deleting cache entries from oldest to newest, until
    // cache usage is brought below limits.
    evictDiskCacheEntries();
#endif

    // XXX write out persistent information about the cache.
    writeCacheInfo();

    // XXX release the reference to the cached file transport service.
    gFileTransportService = nsnull;
}

nsresult
nsDiskCacheDevice::Init()
{
    nsresult rv = installObservers(this);
    if (NS_FAILED(rv)) return rv;
    
    rv = mBoundEntries.Init();
    if (NS_FAILED(rv)) return rv;

    // XXX cache the file transport service to avoid repeated round trips
    // through the service manager.
    gFileTransportService = do_GetService("@mozilla.org/network/file-transport-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    
    // XXX read in persistent information about the cache. this can fail, if
    // no cache directory has ever existed before.
    rv = readCacheInfo();
    if (NS_FAILED(rv)) {
        nsCOMPtr<nsISupportsArray> entries;
        scanDiskCacheEntries(getter_AddRefs(entries));
    }
    
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


nsCacheEntry *
nsDiskCacheDevice::FindEntry(nsCString * key)
{
    // XXX look in entry hashtable first, if not found, then look on
    // disk, to see if we have a disk cache entry that maches.
    nsCacheEntry * entry = nsnull;
    nsDiskCacheEntry * diskEntry = mBoundEntries.GetEntry(key->get());
    if (!diskEntry) {
        nsresult rv = readDiskCacheEntry(key->get(), &diskEntry);
        if (NS_FAILED(rv)) return nsnull;
        entry = diskEntry->getCacheEntry();
        rv = mBoundEntries.AddEntry(diskEntry);
        if (NS_FAILED(rv)) {
            delete entry;
            return nsnull;
        }
    } else {
        // XXX need to make sure this is an exact match, not just
        // a hash code match.
        entry = diskEntry->getCacheEntry();
        if (nsCRT::strcmp(entry->Key()->get(), key->get()) != 0)
            return nsnull;
    }
    return entry;
}


nsresult
nsDiskCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    if (!entry->IsDoomed()) {
        nsDiskCacheEntry * diskEntry = mBoundEntries.GetEntry(entry->Key()->get());
        NS_ASSERTION(diskEntry, "DeactivateEntry called for an entry we don't have!");
        if (!diskEntry)
            return NS_ERROR_INVALID_POINTER;

        // commit any changes about this entry to disk.
        updateDiskCacheEntry(diskEntry);

        // XXX eventually, as a performance enhancement, keep entries around for a while before deleting them.
        // XXX right now, to prove correctness, destroy the entries eagerly.
        mBoundEntries.RemoveEntry(diskEntry);

        // XXX if this entry collided with other concurrently bound entries, then its
        // generation count will be non-zero. The other entries that came before it
        // will be linked to it and doomed. deletion of the entry can only be done
        // when all of the other doomed entries are deactivated, so that the final live entry
        // can have its generation number reset to zero.
        if (diskEntry->getGeneration() != 0)
            scavengeDiskCacheEntries(diskEntry);
        else
            delete entry;
    } else {
        // obliterate all knowledge of this entry on disk.
        nsDiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
        NS_ASSERTION(diskEntry, "nsDiskCacheDevice::DeactivateEntry");
        deleteDiskCacheEntry(diskEntry);
        
        // XXX if this entry resides on a list, then there must have been a collision
        // during the entry's lifetime. use this deactivation as a trigger to scavenge
        // generation numbers, and reset the live entry's generation to zero.
        if (!PR_CLIST_IS_EMPTY(diskEntry)) {
            scavengeDiskCacheEntries(diskEntry);
        }
        
        // delete entry from memory.
        delete entry;
    }

    return NS_OK;
}


nsresult
nsDiskCacheDevice::BindEntry(nsCacheEntry * newEntry)
{
    nsresult rv;

    // Make sure this entry has its associated nsDiskCacheEntry data attached.
    nsDiskCacheEntry* newDiskEntry = ensureDiskCacheEntry(newEntry);
    NS_ASSERTION(newDiskEntry, "nsDiskCacheDevice::BindEntry");
    
    // XXX check for cache collision. if an entry exists on disk that has the same
    // hash code as this newly bound entry, AND there is already a bound entry for
    // that key, we need to ask the Cache service to doom that entry, since two
    // simultaneous entries that have the same hash code aren't allowed until
    // some sort of chaining mechanism is implemented.
    nsDiskCacheEntry* oldDiskEntry = mBoundEntries.GetEntry(newEntry->Key()->get());
    if (oldDiskEntry) {
        // set the generation count on the newly bound entry,
        // so that files created will be unique and won't conflict
        // with the doomed entries that are still active.
        if (oldDiskEntry) {
            newDiskEntry->setGeneration(oldDiskEntry->getGeneration() + 1);
            PR_APPEND_LINK(newDiskEntry, oldDiskEntry);
        }
        
        // XXX Whom do we tell about this impending doom?
        nsCacheService::GlobalInstance()->DoomEntry_Locked(oldDiskEntry->getCacheEntry());
    }

    rv = mBoundEntries.AddEntry(newDiskEntry);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 dataSize = newEntry->DataSize();
    if (dataSize)
        OnDataSizeChange(newEntry, dataSize);

    // XXX Need to make this entry known to other entries?
    return updateDiskCacheEntry(newDiskEntry);
}


void
nsDiskCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    // so it can't be seen by FindEntry() ever again.
    nsDiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
    mBoundEntries.RemoveEntry(diskEntry);

    // keep track of the cache total size.
    mCacheSize -= entry->DataSize();
}


nsresult
nsDiskCacheDevice::GetTransportForEntry(nsCacheEntry * entry,
                                        nsCacheAccessMode mode, 
                                        nsITransport ** result)
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    nsDiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
    if (!diskEntry) return NS_ERROR_OUT_OF_MEMORY;

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    nsCOMPtr<nsITransport>& transport = diskEntry->getTransport(mode);
    if (transport) {
        NS_ADDREF(*result = transport);
        return NS_OK;
    }
#else
    nsCOMPtr<nsITransport> transport;
#endif
    
    // XXX generate the name of the cache entry from the hash code of its key,
    // modulo the number of files we're willing to keep cached.
    nsCOMPtr<nsIFile> dataFile;
    nsresult rv = getFileForDiskCacheEntry(diskEntry, PR_FALSE,
                                           getter_AddRefs(dataFile));
    if (NS_SUCCEEDED(rv)) {
        rv = getTransportForFile(dataFile, mode, getter_AddRefs(transport));
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = transport);
    }
    return rv;
}

nsresult
nsDiskCacheDevice::GetFileForEntry(nsCacheEntry *    entry,
                                   nsIFile **        result)
{
    nsDiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
    return getFileForKey(entry->Key()->get(), PR_FALSE,
                         diskEntry->getGeneration(), result);
}

/**
 * This routine will get called every time an open descriptor.
 */
nsresult
nsDiskCacheDevice::OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize)
{
    mCacheSize += deltaSize;

#if 0    
    if (mCacheSize > mCacheCapacity) {
        // XXX go toss out some disk cache entries.
        evictDiskCacheEntries();
    }
#endif
    
    return NS_OK;
}

nsresult
nsDiskCacheDevice::Visit(nsICacheVisitor * visitor)
{
    // XXX
    nsDiskCacheDeviceInfo* deviceInfo = new nsDiskCacheDeviceInfo(this);
    nsCOMPtr<nsICacheDeviceInfo> ref(deviceInfo);
    
    PRBool keepGoing;
    nsresult rv = visitor->VisitDevice(DISK_CACHE_DEVICE_ID, deviceInfo, &keepGoing);
    if (NS_FAILED(rv)) return rv;
    
    if (keepGoing)
        return visitEntries(visitor);

    return NS_OK;
}

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

void nsDiskCacheDevice::setCacheCapacity(PRUint32 capacity)
{
    // XXX start evicting entries if the new size is smaller!
    mCacheCapacity = capacity;
}

PRUint32 nsDiskCacheDevice::getCacheCapacity()
{
    return mCacheCapacity;
}

PRUint32 nsDiskCacheDevice::getCacheSize()
{
    return mCacheSize;
}

nsresult nsDiskCacheDevice::getFileForKey(const char* key, PRBool meta,
                                          PRUint32 generation, nsIFile ** result)
{
    if (mCacheDirectory) {
        nsCOMPtr<nsIFile> entryFile;
        nsresult rv = mCacheDirectory->Clone(getter_AddRefs(entryFile));
    	if (NS_FAILED(rv))
    		return rv;
        // generate the hash code for this entry, and use that as a file name.
        char name[32];
        PLDHashNumber hash = ::PL_DHashStringKey(NULL, key);
        ::sprintf(name, "%08X%c%02X", hash, (meta ? 'm' : 'd'), generation);
        entryFile->Append(name);
        NS_ADDREF(*result = entryFile);
        return NS_OK;
    }
    return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsDiskCacheDevice::getFileForDiskCacheEntry(nsDiskCacheEntry * diskEntry, PRBool meta,
                                                     nsIFile ** result)
{
    if (mCacheDirectory) {
        nsCOMPtr<nsIFile> entryFile;
        nsresult rv = mCacheDirectory->Clone(getter_AddRefs(entryFile));
    	if (NS_FAILED(rv))
    		return rv;
        // generate the hash code for this entry, and use that as a file name.
        char name[32];
        ::sprintf(name, "%08X%c%02X", diskEntry->getHashNumber(),
                  (meta ? 'm' : 'd'), diskEntry->getGeneration());
        entryFile->Append(name);
        NS_ADDREF(*result = entryFile);
        return NS_OK;
    }
    return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsDiskCacheDevice::getTransportForFile(nsIFile* file, nsCacheAccessMode mode, nsITransport ** result)
{
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
    return gFileTransportService->CreateTransport(file, ioFlags, PR_IRUSR | PR_IWUSR, result);
}

inline PRBool isMetaDataFile(const char* name)
{
    return (name[8] == 'm');
}

// XXX All these transports and opening/closing of files. We need a way to cache open files,
// XXX and to seek. Perhaps I should just be using ANSI FILE objects for all of the metadata
// XXX operations.

nsresult nsDiskCacheDevice::visitEntries(nsICacheVisitor * visitor)
{
    nsCOMPtr<nsISimpleEnumerator> entries;
    nsresult rv = mCacheDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_FAILED(rv)) return rv;
    
    nsDiskCacheEntryInfo* entryInfo = new nsDiskCacheEntryInfo();
    if (!entryInfo) return NS_ERROR_OUT_OF_MEMORY;
    nsCOMPtr<nsICacheEntryInfo> ref(entryInfo);
    
    for (PRBool more; NS_SUCCEEDED(entries->HasMoreElements(&more)) && more;) {
        nsCOMPtr<nsISupports> next;
        rv = entries->GetNext(getter_AddRefs(next));
        if (NS_FAILED(rv)) break;
        nsCOMPtr<nsIFile> file(do_QueryInterface(next, &rv));
        if (NS_FAILED(rv)) break;
        nsXPIDLCString name;
        rv = file->GetLeafName(getter_Copies(name));
        if (isMetaDataFile(name)) {
            // this must be a metadata file.
            nsCOMPtr<nsITransport> transport;
            rv = getTransportForFile(file, nsICache::ACCESS_READ, getter_AddRefs(transport));
            if (NS_FAILED(rv)) continue;
            nsCOMPtr<nsIInputStream> input;
            rv = transport->OpenInputStream(0, ULONG_MAX, 0, getter_AddRefs(input));
            if (NS_FAILED(rv)) continue;
            
            // read the metadata file.
            rv = entryInfo->Read(input);
            input->Close();
            if (NS_FAILED(rv)) break;
            
            // tell the visitor about this entry.
            PRBool keepGoing;
            rv = visitor->VisitEntry(DISK_CACHE_DEVICE_ID, entryInfo, &keepGoing);
            if (NS_FAILED(rv)) return rv;
            if (!keepGoing) break;
        }
    }

    return NS_OK;
}

class UpdateEntryVisitor : public nsDiskCacheEntryHashTable::Visitor {
    nsDiskCacheDevice* mDevice;
public:
    UpdateEntryVisitor(nsDiskCacheDevice * device) : mDevice(device) {}
    
    virtual PRBool VisitEntry(nsDiskCacheEntry * diskEntry)
    {
        mDevice->updateDiskCacheEntry(diskEntry);
        return PR_TRUE;
    }
};

nsresult nsDiskCacheDevice::updateDiskCacheEntries()
{
    UpdateEntryVisitor visitor(this);
    mBoundEntries.VisitEntries(&visitor);
    return NS_OK;
}

nsresult nsDiskCacheDevice::updateDiskCacheEntry(nsDiskCacheEntry* diskEntry)
{
    nsCacheEntry* entry = diskEntry->getCacheEntry();
    if (entry->IsMetaDataDirty() || entry->IsEntryDirty()) {
        nsresult rv;
#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
        nsCOMPtr<nsITransport>& transport = diskEntry->getMetaTransport(nsICache::ACCESS_WRITE);
#else
        nsCOMPtr<nsITransport> transport;
#endif
        if (!transport) {
            nsCOMPtr<nsIFile> file;
            rv = getFileForDiskCacheEntry(diskEntry, PR_TRUE,
                                     getter_AddRefs(file));
            if (NS_FAILED(rv)) return rv;
            
            rv = getTransportForFile(file, nsICache::ACCESS_WRITE,
                                     getter_AddRefs(transport));
            if (NS_FAILED(rv)) return rv;
        }

        nsCOMPtr<nsIOutputStream> output;
        rv = transport->OpenOutputStream(0, ULONG_MAX, 0, getter_AddRefs(output));
        if (NS_FAILED(rv)) return rv;
        
        // write the metadata to the file.
        MetaDataFile metaDataFile(entry);
        rv = metaDataFile.Init(entry);
        if (NS_FAILED(rv)) return rv;
        rv = metaDataFile.Write(output);
        
        rv = output->Close();
        
        // mark the disk entry as being consistent with meta data file.
        entry->MarkMetaDataClean();
        entry->MarkEntryClean();
    }
    return NS_OK;
}

static nsresult NS_NewCacheEntry(nsCacheEntry ** result,
                                 const char * key,
                                 PRBool streamBased,
                                 nsCacheStoragePolicy storagePolicy,
                                 nsCacheDevice* device)
{
    nsCString* newKey = new nsCString(key);
    if (!newKey) return NS_ERROR_OUT_OF_MEMORY;
    
    nsCacheEntry* entry = new nsCacheEntry(newKey, streamBased, storagePolicy);
    if (!entry) { delete newKey; return NS_ERROR_OUT_OF_MEMORY; }
    
    entry->SetCacheDevice(device);
    
    *result = entry;
    return NS_OK;
}

nsresult nsDiskCacheDevice::readDiskCacheEntry(const char * key, nsDiskCacheEntry ** result)
{
    // result should be nsull on cache miss.
    *result = nsnull;

    // up front, check to see if the file we are looking for exists.
    nsCOMPtr<nsIFile> file;
    nsresult rv = getFileForKey(key, PR_TRUE, 0, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    PRBool exists;
    rv = file->Exists(&exists);
    if (NS_FAILED(rv) || !exists) return NS_ERROR_NOT_AVAILABLE;

    nsCacheEntry* entry;
    rv = NS_NewCacheEntry(&entry, key, PR_TRUE, nsICache::STORE_ON_DISK, this);
    if (NS_FAILED(rv)) return rv;
    
    do {
        nsDiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
        if (!diskEntry) break;

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
        nsCOMPtr<nsITransport>& transport = diskEntry->getMetaTransport(nsICache::ACCESS_READ);
#else
        nsCOMPtr<nsITransport> transport;
#endif
        if (!transport) {
            rv = getTransportForFile(file, nsICache::ACCESS_READ, getter_AddRefs(transport));
            if (NS_FAILED(rv)) break;
        }

        nsCOMPtr<nsIInputStream> input;
        rv = transport->OpenInputStream(0, ULONG_MAX, 0, getter_AddRefs(input));
        if (NS_FAILED(rv)) break;
        
        // read the metadata file.
        MetaDataFile metaDataFile;
        rv = metaDataFile.Read(input);
        input->Close();
        if (NS_FAILED(rv)) break;
        
        // Ensure that the keys match.
        if (nsCRT::strcmp(key, metaDataFile.mKey) != 0) break;
        
        // initialize the entry.
        entry->SetFetchCount(metaDataFile.mFetchCount);
        entry->SetLastFetched(metaDataFile.mLastFetched);
        entry->SetLastModified(metaDataFile.mLastModified);
        entry->SetExpirationTime(metaDataFile.mExpirationTime);
        entry->SetDataSize(metaDataFile.mDataSize);
        
        // restore the metadata.
        if (metaDataFile.mMetaDataSize) {
            rv = entry->UnflattenMetaData(metaDataFile.mMetaData, metaDataFile.mMetaDataSize);
            if (NS_FAILED(rv)) break;
        }
        
        // celebrate!        
        *result = diskEntry;
        return NS_OK;
    } while (0);

    // oh, auto_ptr<> would be nice right about now.    
    delete entry;
    return NS_ERROR_NOT_AVAILABLE;
}

nsresult nsDiskCacheDevice::deleteDiskCacheEntry(nsDiskCacheEntry * diskEntry)
{
    nsresult rv;
    
    // delete the metadata file.
    nsCOMPtr<nsIFile> metaFile;
    rv = getFileForDiskCacheEntry(diskEntry, PR_TRUE,
                                  getter_AddRefs(metaFile));
    if (NS_SUCCEEDED(rv)) {
        rv = metaFile->Delete(PR_FALSE);
        // NS_ASSERTION(NS_SUCCEEDED(rv), "nsDiskCacheDevice::deleteDiskCacheEntry");
    }
    
    // delete the data file
    nsCOMPtr<nsIFile> dataFile;
    rv = getFileForDiskCacheEntry(diskEntry, PR_FALSE,
                                  getter_AddRefs(dataFile));
    if (NS_SUCCEEDED(rv)) {
        rv = dataFile->Delete(PR_FALSE);
        // NS_ASSERTION(NS_SUCCEEDED(rv), "nsDiskCacheDevice::deleteDiskCacheEntry");
    }
    
    return NS_OK;
}


nsresult nsDiskCacheDevice::scavengeDiskCacheEntries(nsDiskCacheEntry * diskEntry)
{
    nsresult rv;
    
    // count the number of doomed entries still in the list, not
    // including the passed-in entry. if this number is zero, then
    // the liveEntry, if inactive, can have its generation reset
    // to zero.
    PRUint32 doomedEntryCount = 0;
    nsDiskCacheEntry * youngestDiskEntry = diskEntry;
    nsCacheEntry * youngestEntry = diskEntry->getCacheEntry();
    nsDiskCacheEntry * nextDiskEntry = NS_STATIC_CAST(nsDiskCacheEntry*, PR_NEXT_LINK(diskEntry));
    while (nextDiskEntry != diskEntry) {
        nsCacheEntry* nextEntry = nextDiskEntry->getCacheEntry();
        if (nextEntry->IsDoomed()) {
            ++doomedEntryCount;
        } else if (nextDiskEntry->getGeneration() > youngestDiskEntry->getGeneration()) {
            youngestEntry = nextEntry;
            youngestDiskEntry = nextDiskEntry;
        }
        nextDiskEntry = NS_STATIC_CAST(nsDiskCacheEntry*, PR_NEXT_LINK(nextDiskEntry));
    }
    
    if (doomedEntryCount == 0 && !youngestEntry->IsDoomed() && !youngestEntry->IsActive()) {
        PRUint32 generation = youngestDiskEntry->getGeneration();
        // XXX reset generation number.
        const char* key = youngestEntry->Key()->get();
        nsCOMPtr<nsIFile> oldFile, newFile;
        nsXPIDLCString newName;

        // rename metadata file.
        rv = getFileForKey(key, PR_TRUE, generation, getter_AddRefs(oldFile));
        if (NS_FAILED(rv)) return rv;
        rv = getFileForKey(key, PR_TRUE, 0, getter_AddRefs(newFile));
        rv = newFile->GetLeafName(getter_Copies(newName));
        if (NS_FAILED(rv)) return rv;
        rv = oldFile->MoveTo(nsnull, newName);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsDiskCacheDevice::scavengeDiskCacheEntries");

        // rename data file.
        rv = getFileForKey(key, PR_FALSE, generation, getter_AddRefs(oldFile));
        if (NS_FAILED(rv)) return rv;
        rv = getFileForKey(key, PR_FALSE, 0, getter_AddRefs(newFile));
        rv = newFile->GetLeafName(getter_Copies(newName));
        if (NS_FAILED(rv)) return rv;
        rv = oldFile->MoveTo(nsnull, newName);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsDiskCacheDevice::scavengeDiskCacheEntries");

        // delete the youngestEntry, otherwise nobody else will.
        delete youngestEntry;
    }
    
    return NS_OK;
}

/**
 * Helper class for implementing do_QueryElementAt() pattern. Thanks scc!
 * Eventually this should become part of nsICollection.idl.
 */
class nsQueryElementAt : public nsCOMPtr_helper {
public:
    nsQueryElementAt( nsICollection* aCollection, PRUint32 aIndex, nsresult* aErrorPtr )
        :   mCollection(aCollection),
            mIndex(aIndex),
            mErrorPtr(aErrorPtr)
    {
        // nothing else to do here
    }

    virtual nsresult operator()( const nsIID& aIID, void** aResult) const
    {
        nsresult status;
        if ( mCollection ) {
            if ( !NS_SUCCEEDED(status = mCollection->QueryElementAt(mIndex, aIID, aResult)) )
            *aResult = 0;
        } else
            status = NS_ERROR_NULL_POINTER;
        if ( mErrorPtr )
            *mErrorPtr = status;
        return status;
    }
    
private:
      nsICollection*  mCollection;
      PRUint32        mIndex;
      nsresult*       mErrorPtr;
};

inline const nsQueryElementAt
do_QueryElementAt( nsICollection* aCollection, PRUint32 aIndex, nsresult* aErrorPtr = 0 )
{
    return nsQueryElementAt(aCollection, aIndex, aErrorPtr);
}

nsresult nsDiskCacheDevice::scanDiskCacheEntries(nsISupportsArray ** result)
{
    nsresult rv;
    
    // XXX make sure meta data is up to date.
    rv = updateDiskCacheEntries();
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> files;
    rv = mCacheDirectory->GetDirectoryEntries(getter_AddRefs(files));
    if (NS_FAILED(rv)) return rv;
    
    nsCOMPtr<nsISupportsArray> entries = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    PRUint32 newCacheSize = 0;
    
    for (PRBool more; NS_SUCCEEDED(files->HasMoreElements(&more)) && more;) {
        nsCOMPtr<nsISupports> next;
        rv = files->GetNext(getter_AddRefs(next));
        if (NS_FAILED(rv)) break;
        nsCOMPtr<nsIFile> file(do_QueryInterface(next, &rv));
        if (NS_FAILED(rv)) break;
        nsXPIDLCString name;
        rv = file->GetLeafName(getter_Copies(name));
        if (isMetaDataFile(name)) {
            // this must be a metadata file.
            nsCOMPtr<nsITransport> transport;
            rv = getTransportForFile(file, nsICache::ACCESS_READ, getter_AddRefs(transport));
            if (NS_FAILED(rv)) continue;
            nsCOMPtr<nsIInputStream> input;
            rv = transport->OpenInputStream(0, ULONG_MAX, 0, getter_AddRefs(input));
            if (NS_FAILED(rv)) continue;

            nsDiskCacheEntryInfo* entryInfo = new nsDiskCacheEntryInfo();
            if (!entryInfo) return NS_ERROR_OUT_OF_MEMORY;
            nsCOMPtr<nsISupports> ref(entryInfo);
            rv = entryInfo->Read(input);
            input->Close();
            if (NS_FAILED(rv)) return rv;

            // update the cache size.
            PRUint32 dataSize;
            entryInfo->GetDataSize(&dataSize);
            newCacheSize += dataSize;
            
            // sort entries by modification time.
            PRUint32 count;
            entries->Count(&count);
            if (count == 0) {
                rv = entries->AppendElement(entryInfo);
                if (NS_FAILED(rv)) return rv;
                continue;
            }
            PRUint32 modTime;
            entryInfo->GetLastModified(&modTime);
            PRInt32 low = 0, high = count - 1;
            for (;;) {
                PRInt32 middle = (low + high) / 2;
                nsCOMPtr<nsICacheEntryInfo> info = do_QueryElementAt(entries, middle, &rv);
                if (NS_FAILED(rv)) return rv;
                PRUint32 modTime1;
                info->GetLastModified(&modTime1);
                if (low >= high) {
                    if (modTime <= modTime1)
                        rv = entries->InsertElementAt(entryInfo, middle);
                    else
                        rv = entries->InsertElementAt(entryInfo, middle + 1);
                    if (NS_FAILED(rv)) return rv;
                    break;
                } else {
                    if (modTime < modTime1) {
                        high = middle - 1;
                    } else if (modTime > modTime1) {
                        low = middle + 1;
                    } else {
                        rv = entries->InsertElementAt(entryInfo, middle);
                        if (NS_FAILED(rv)) return rv;
                        break;
                    }
                }
            }
        }
    }

    NS_ADDREF(*result = entries);

    // we've successfully totaled the cache size.
    mCacheSize = newCacheSize;
    
    return NS_OK;
}

nsresult nsDiskCacheDevice::evictDiskCacheEntries()
{
    nsCOMPtr<nsISupportsArray> entries;
    nsresult rv = scanDiskCacheEntries(getter_AddRefs(entries));
    if (NS_FAILED(rv)) return rv;

    if (mCacheSize < mCacheCapacity) return NS_OK;
    
    // these are sorted in oldest to newest order.
    PRUint32 count;
    entries->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
        nsCOMPtr<nsICacheEntryInfo> info = do_QueryElementAt(entries, i, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsDiskCacheEntryInfo* entryInfo = (nsDiskCacheEntryInfo*) info.get();
            const char* key = entryInfo->Key();
            
            // XXX if this entry is currently active, then leave it alone,
            // as it is likely to be modified very soon.
            nsDiskCacheEntry* diskEntry = mBoundEntries.GetEntry(key);
            if (diskEntry) continue;
            
            // delete the metadata file.
            nsCOMPtr<nsIFile> metaFile;
            rv = getFileForKey(key, PR_TRUE, 0, getter_AddRefs(metaFile));
            if (NS_SUCCEEDED(rv)) {
                rv = metaFile->Delete(PR_FALSE);
            }
            
            // delete the data file
            nsCOMPtr<nsIFile> dataFile;
            rv = getFileForKey(key, PR_FALSE, 0, getter_AddRefs(dataFile));
            if (NS_SUCCEEDED(rv)) {
                rv = dataFile->Delete(PR_FALSE);
            }

            // update the cache size.
            PRUint32 dataSize;
            info->GetDataSize(&dataSize);
            mCacheSize -= dataSize;
            if (mCacheSize <= mCacheCapacity)
                break;
        }
    }
    
    return NS_OK;
}

static FILE* openFileStream(nsIFile* file, const char* mode)
{
    FILE* stream = nsnull;
    nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(file);
    if (localFile) {
        nsresult rv = localFile->OpenANSIFileDesc(mode, &stream);
        if (NS_FAILED(rv)) return nsnull;
    }
    return stream;
}

nsresult nsDiskCacheDevice::writeCacheInfo()
{
    nsCOMPtr<nsIFile> infoFile;
    nsresult rv = mCacheDirectory->Clone(getter_AddRefs(infoFile));
    if (NS_FAILED(rv)) return rv;
    
    rv = infoFile->Append("CacheInfo");
    if (NS_FAILED(rv)) return rv;
    
    FILE* stream = openFileStream(infoFile, "wb");
    if (stream) {
        fwrite(&mCacheSize, sizeof(mCacheSize), 1, stream);
        fclose(stream);
    } else {
        nsCOMPtr<nsITransport> transport;
        rv = getTransportForFile(infoFile, nsICache::ACCESS_WRITE, getter_AddRefs(transport));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIOutputStream> output;
        rv = transport->OpenOutputStream(0, ULONG_MAX, 0, getter_AddRefs(output));
        if (NS_FAILED(rv)) return rv;
        PRUint32 count = sizeof(mCacheSize);
        rv = output->Write((char*)&mCacheSize, count, &count);
        rv = output->Close();
    }
    
    return NS_OK;
}

nsresult nsDiskCacheDevice::readCacheInfo()
{
    nsCOMPtr<nsIFile> infoFile;
    nsresult rv = mCacheDirectory->Clone(getter_AddRefs(infoFile));
    if (NS_FAILED(rv)) return rv;
    
    rv = infoFile->Append("CacheInfo");
    if (NS_FAILED(rv)) return rv;
    
    FILE* stream = openFileStream(infoFile, "rb");
    if (stream) {
        fread(&mCacheSize, sizeof(mCacheSize), 1, stream);
        fclose(stream);
    } else {
        nsCOMPtr<nsITransport> transport;
        rv = getTransportForFile(infoFile, nsICache::ACCESS_READ, getter_AddRefs(transport));
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIInputStream> input;
        rv = transport->OpenInputStream(0, ULONG_MAX, 0, getter_AddRefs(input));
        if (NS_FAILED(rv)) return rv;
        PRUint32 count = sizeof(mCacheSize);
        rv = input->Read((char*)&mCacheSize, count, &count);
        rv = input->Close();
    }

    return NS_OK;
}
