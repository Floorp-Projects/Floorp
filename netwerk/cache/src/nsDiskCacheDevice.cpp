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
#include "nsCacheService.h"

#include "nsICacheService.h"
#include "nsIFileTransportService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsICacheVisitor.h"
#include "nsXPIDLString.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"

static const char DISK_CACHE_DEVICE_ID[] = { "disk" };

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

// Using nsIPref will be subsumed with nsIDirectoryService when a selector
// for the cache directory has been defined.

#include "nsIPref.h"

static const char CACHE_DIR_PREF[] = { "browser.newcache.directory" };
static const char CACHE_DISK_CAPACITY[] = { "browser.cache.disk_cache_size" };

static int PR_CALLBACK cacheDirectoryChanged(const char *pref, void *closure)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if (NS_FAILED(rv))
		return rv;
    
	nsCOMPtr<nsILocalFile> cacheDirectory;
    rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs( cacheDirectory ));
	if (NS_FAILED(rv))
		return rv;

    nsDiskCacheDevice* device = NS_STATIC_CAST(nsDiskCacheDevice*, closure);
    device->setCacheDirectory(cacheDirectory);
    
    return NS_OK;
}

static int PR_CALLBACK cacheCapacityChanged(const char *pref, void *closure)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if (NS_FAILED(rv))
		return rv;
    
    PRInt32 cacheCapacity;
    rv = prefs->GetIntPref(CACHE_DISK_CAPACITY, &cacheCapacity);
	if (NS_FAILED(rv))
		return rv;

    nsDiskCacheDevice* device = NS_STATIC_CAST(nsDiskCacheDevice*, closure);
    device->setCacheCapacity(cacheCapacity);
    
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

static nsresult installPrefListeners(nsDiskCacheDevice* device)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if (NS_FAILED(rv))
		return rv;

	rv = prefs->RegisterCallback(CACHE_DISK_CAPACITY, cacheCapacityChanged, device);
	if (NS_FAILED(rv))
		return rv;

    PRInt32 cacheCapacity;
    rv = prefs->GetIntPref(CACHE_DISK_CAPACITY, &cacheCapacity);
    if (NS_FAILED(rv)) {
#if DEBUG
        const PRInt32 kTenMegabytes = 10 * 1024 * 1024;
        rv = prefs->SetIntPref(CACHE_DISK_CAPACITY, kTenMegabytes);
#else
		return rv;
#endif
    } else {
        device->setCacheCapacity(cacheCapacity);
    }

	rv = prefs->RegisterCallback(CACHE_DIR_PREF, cacheDirectoryChanged, device);
	if (NS_FAILED(rv))
		return rv;

	nsCOMPtr<nsILocalFile> cacheDirectory;
    rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs(cacheDirectory));
    if (NS_FAILED(rv)) {
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

        rv = ensureCacheDirectory(cacheDirectory);
        if (NS_FAILED(rv))
            return rv;

        rv = prefs->SetFileXPref(CACHE_DIR_PREF, cacheDirectory);
    	if (NS_FAILED(rv))
    		return rv;
#else
        return rv;
#endif
    } else {
        // always make sure the directory exists, the user may blow it away.
        rv = ensureCacheDirectory(cacheDirectory);
        if (NS_FAILED(rv))
            return rv;

        // cause the preference to be set up initially.
        device->setCacheDirectory(cacheDirectory);
    }
    
    return NS_OK;
}

static nsresult removePrefListeners(nsDiskCacheDevice* device)
{
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if ( NS_FAILED (rv ) )
		return rv;

	rv = prefs->UnregisterCallback(CACHE_DIR_PREF, cacheDirectoryChanged, device);
	if ( NS_FAILED( rv ) )
		return rv;

    return NS_OK;
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

class DiskCacheEntry : public PRCList, public nsISupports {
public:
    NS_DECL_ISUPPORTS

    DiskCacheEntry(nsCacheEntry* entry)
        :   mCacheEntry(entry),
            mGeneration(0)
    {
        NS_INIT_ISUPPORTS();
        PR_INIT_CLIST(this);
        mHashNumber = ::PL_DHashStringKey(NULL, entry->Key()->get());
    }

    virtual ~DiskCacheEntry()
    {
        PR_REMOVE_LINK(this);
    }

#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    /**
     * Maps a cache access mode to a cached nsITransport for that access
     * mode. We keep these cached to avoid repeated trips to the
     * file transport service.
     */
    nsCOMPtr<nsITransport>& getTransport(nsCacheAccessMode mode)
    {
        return mTransports[mode - 1];
    }
    
    nsCOMPtr<nsITransport>& getMetaTransport(nsCacheAccessMode mode)
    {
        return mMetaTransports[mode - 1];
    }
#endif
    
    nsCacheEntry* getCacheEntry()
    {
        return mCacheEntry;
    }
    
    PRUint32 getGeneration()
    {
        return mGeneration;
    }
    
    void setGeneration(PRUint32 generation)
    {
        mGeneration = generation;
    }
    
    PLDHashNumber getHashNumber()
    {
        return mHashNumber;
    }
    
private:
#ifdef MOZ_NEW_CACHE_REUSE_TRANSPORTS
    nsCOMPtr<nsITransport>  mTransports[3];
    nsCOMPtr<nsITransport>  mMetaTransports[3];
#endif
    nsCacheEntry*           mCacheEntry;
    PRUint32                mGeneration;
    PLDHashNumber           mHashNumber;
};
NS_IMPL_ISUPPORTS0(DiskCacheEntry);

static DiskCacheEntry*
ensureDiskCacheEntry(nsCacheEntry * entry)
{
    nsCOMPtr<nsISupports> data;
    nsresult rv = entry->GetData(getter_AddRefs(data));
    if (NS_SUCCEEDED(rv) && !data) {
        DiskCacheEntry* diskEntry = new DiskCacheEntry(entry);
        data = diskEntry;
        if (NS_SUCCEEDED(rv) && data)
            entry->SetData(data.get());
    }
    return (DiskCacheEntry*) (nsISupports*) data.get();
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
    PRUint32        mLastValidated;     // NOT NEEDED
    PRUint32        mExpirationTime;
    PRUint32        mDataSize;
    PRUint32        mKeySize;
    PRUint32        mMetaDataSize;
    // followed by null-terminated key and metadata string values.

    MetaDataHeader()
        :   mHeaderSize(sizeof(MetaDataHeader)),
            mFetchCount(0),
            mLastFetched(0),
            mLastValidated(0),
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
            mLastValidated(entry->LastValidated()),
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

class nsCacheEntryInfo : public nsICacheEntryInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYINFO

    nsCacheEntryInfo()
    {
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsCacheEntryInfo() {}
    
    nsresult Read(nsIInputStream * input)
    {
        nsresult rv = mMetaDataFile.Read(input);
        if (NS_FAILED(rv)) return rv;
        mClientID = mMetaDataFile.mKey;
        char* colon = ::strchr(mClientID, ':');
        if (!colon) return NS_ERROR_FAILURE;
        *colon = '\0';
        mKey = colon + 1;
        return NS_OK;
    }
    
    const char* ClientID() { return mClientID; }
    
private:
    MetaDataFile mMetaDataFile;
    char* mClientID;
    char* mKey;
};
NS_IMPL_ISUPPORTS1(nsCacheEntryInfo, nsICacheEntryInfo);

NS_IMETHODIMP nsCacheEntryInfo::GetClientID(char * *aClientID)
{
    char * result = nsCRT::strdup(mClientID);
    if (!result) return NS_ERROR_OUT_OF_MEMORY;
    *aClientID = result;
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetKey(char * *aKey)
{
    char * result = nsCRT::strdup(mKey);
    if (!result) return NS_ERROR_OUT_OF_MEMORY;
    *aKey = result;
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetFetchCount(PRInt32 *aFetchCount)
{
    return *aFetchCount = mMetaDataFile.mFetchCount;
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetLastFetched(PRTime *aLastFetched)
{
    *aLastFetched = ConvertSecondsToPRTime(mMetaDataFile.mLastFetched);
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetLastModified(PRTime *aLastModified)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCacheEntryInfo::GetLastValidated(PRTime *aLastValidated)
{
    *aLastValidated = ConvertSecondsToPRTime(mMetaDataFile.mLastValidated);
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetExpirationTime(PRTime *aExpirationTime)
{
    *aExpirationTime = ConvertSecondsToPRTime(mMetaDataFile.mExpirationTime);
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetStreamBased(PRBool *aStreamBased)
{
    *aStreamBased = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsCacheEntryInfo::GetDataSize(PRUint32 *aDataSize)
{
    *aDataSize = mMetaDataFile.mDataSize;
    return NS_OK;
}

// XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

nsDiskCacheDevice::nsDiskCacheDevice()
    :   mCacheCapacity(0), mCacheSize(0)
{
}

nsDiskCacheDevice::~nsDiskCacheDevice()
{
    removePrefListeners(this);
}

nsresult
nsDiskCacheDevice::Init()
{
    nsresult rv = installPrefListeners(this);
    if (NS_FAILED(rv)) return rv;
    
    rv = mBoundEntries.Init();
    if (NS_FAILED(rv)) return rv;
    
    return  NS_OK;
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
    nsCacheEntry * entry = mBoundEntries.GetEntry(key);
    if (!entry) {
        nsresult rv = readDiskCacheEntry(key, &entry);
        if (NS_FAILED(rv)) return nsnull;
        rv = mBoundEntries.AddEntry(entry);
        if (NS_FAILED(rv)) {
            delete entry;
            return nsnull;
        }
    }

    // XXX find eviction element and move it to the tail of the queue
    return entry;
}


nsresult
nsDiskCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    if (!entry->IsDoomed()) {
        nsCacheEntry * ourEntry = mBoundEntries.GetEntry(entry->Key());
        NS_ASSERTION(ourEntry, "DeactivateEntry called for an entry we don't have!");
        if (!ourEntry)
            return NS_ERROR_INVALID_POINTER;

        // commit any changes about this entry to disk.
        updateDiskCacheEntry(entry);

        // XXX eventually, as a performance enhancement, keep entries around for a while before deleting them.
        // XXX right now, to prove correctness, destroy the entries eagerly.
        mBoundEntries.RemoveEntry(entry);

        // XXX if this entry collided with other concurrently bound entries, then its
        // generation count will be non-zero. The other entries that came before it
        // will be linked to it and doomed. deletion of the entry can only be done
        // when all of the other doomed entries are deactivated, so that the final live entry
        // can have its generation number reset to zero.
        DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
        NS_ASSERTION(diskEntry, "nsDiskCacheDevice::DeactivateEntry");
        if (diskEntry->getGeneration() != 0)
            scavengeDiskCacheEntries(diskEntry);
        else
            delete entry;
    } else {
        // obliterate all knowledge of this entry on disk.
        DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
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

    // Make sure this entry has its associated DiskCacheEntry data attached.
    DiskCacheEntry* newDiskEntry = ensureDiskCacheEntry(newEntry);
    NS_ASSERTION(newDiskEntry, "nsDiskCacheDevice::BindEntry");
    
    // XXX check for cache collision. if an entry exists on disk that has the same
    // hash code as this newly bound entry, AND there is already a bound entry for
    // that key, we need to ask the Cache service to doom that entry, since two
    // simultaneous entries that have the same hash code aren't allowed until
    // some sort of chaining mechanism is implemented.
    nsCacheEntry* oldEntry;
    rv = checkForCollision(newEntry, &oldEntry);
    if (NS_SUCCEEDED(rv) && oldEntry) {
        // set the generation count on the newly bound entry,
        // so that files created will be unique and won't conflict
        // with the doomed entries that are still active.
        DiskCacheEntry* oldDiskEntry = ensureDiskCacheEntry(oldEntry);
        NS_ASSERTION(oldDiskEntry, "nsDiskCacheDevice::BindEntry");
        if (oldDiskEntry) {
            newDiskEntry->setGeneration(oldDiskEntry->getGeneration() + 1);
            PR_APPEND_LINK(oldDiskEntry, newDiskEntry);
        }
                
        // XXX Whom do we tell about this impending doom?
        nsCacheService::GlobalInstance()->DoomEntry_Locked(oldEntry);
    }

    rv = mBoundEntries.AddEntry(newEntry);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 dataSize = newEntry->DataSize();
    if (dataSize)
        OnDataSizeChange(newEntry, dataSize);

    // XXX Need to make this entry known to other entries?
    return updateDiskCacheEntry(newEntry);
}


void
nsDiskCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    // so it can't be seen by FindEntry() ever again.
    mBoundEntries.RemoveEntry(entry);
}


nsresult
nsDiskCacheDevice::GetTransportForEntry(nsCacheEntry * entry,
                                        nsCacheAccessMode mode, 
                                        nsITransport ** result)
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
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
    DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
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

nsresult nsDiskCacheDevice::getFileForDiskCacheEntry(DiskCacheEntry * diskEntry, PRBool meta,
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
    nsresult rv;
    static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
    NS_WITH_SERVICE(nsIFileTransportService, service, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    PRInt32 ioFlags;
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
    return service->CreateTransport(file, ioFlags, PR_IRUSR | PR_IWUSR, result);
}


// XXX All these transports and opening/closing of files. We need a way to cache open files,
// XXX and to seek. Perhaps I should just be using ANSI FILE objects for all of the metadata
// XXX operations.

nsresult nsDiskCacheDevice::visitEntries(nsICacheVisitor * visitor)
{
    nsCOMPtr<nsISimpleEnumerator> entries;
    nsresult rv = mCacheDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_FAILED(rv)) return rv;
    
    nsCacheEntryInfo* entryInfo = new nsCacheEntryInfo();
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
        if (nsCRT::strlen(name) == 9) {
            // this must be a metadata file.
            nsCOMPtr<nsITransport> transport;
            rv = getTransportForFile(file, nsICache::ACCESS_READ, getter_AddRefs(transport));
            if (NS_FAILED(rv)) continue;
            nsCOMPtr<nsIInputStream> input;
            rv = transport->OpenInputStream(0, -1, 0, getter_AddRefs(input));
            if (NS_FAILED(rv)) continue;
            
            // read the metadata file.
            rv = entryInfo->Read(input);
            input->Close();
            if (NS_FAILED(rv)) break;
            
            // tell the visitor about this entry.
            PRBool keepGoing;
            rv = visitor->VisitEntry(DISK_CACHE_DEVICE_ID, entryInfo->ClientID(),
                                     entryInfo, &keepGoing);
            if (NS_FAILED(rv)) return rv;
            if (!keepGoing) break;
        }
    }

    return NS_OK;
}

nsresult nsDiskCacheDevice::updateDiskCacheEntry(nsCacheEntry* entry)
{
    if (entry->IsMetaDataDirty() || entry->IsEntryDirty()) {
        DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
        if (!diskEntry) return NS_ERROR_OUT_OF_MEMORY;
        
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
        rv = transport->OpenOutputStream(0, -1, 0, getter_AddRefs(output));
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
                                 nsCString* key,
                                 PRBool streamBased,
                                 nsCacheStoragePolicy storagePolicy,
                                 nsCacheDevice* device)
{
    nsCString* newKey = new nsCString(key->get());
    if (!newKey) return NS_ERROR_OUT_OF_MEMORY;
    
    nsCacheEntry* entry = new nsCacheEntry(newKey, streamBased, storagePolicy);
    if (!entry) { delete newKey; return NS_ERROR_OUT_OF_MEMORY; }
    
    entry->SetCacheDevice(device);
    
    *result = entry;
    return NS_OK;
}

/*
static nsresult readMetaDataFile(nsIFile* file, DiskCacheEntry* diskEntry, MetaDataFile& metaDataFile)
{
    nsCOMPtr<nsITransport>& transport = diskEntry->getMetaTransport(nsICache::ACCESS_READ);
    if (!transport) {
        rv = getTransportForFile(file, nsICache::ACCESS_READ, getter_AddRefs(transport));
        if (NS_FAILED(rv)) break;
    }

    nsCOMPtr<nsIInputStream> input;
    rv = transport->OpenInputStream(0, -1, 0, getter_AddRefs(input));
    if (NS_FAILED(rv)) break;
    
    // read the metadata file.
    MetaDataFile metaDataFile;
    rv = metaDataFile.Read(input);
    input->Close();
}
*/

nsresult nsDiskCacheDevice::readDiskCacheEntry(nsCString* key, nsCacheEntry ** result)
{
    // up front, check to see if the file we are looking for exists.
    nsCOMPtr<nsIFile> file;
    nsresult rv = getFileForKey(key->get(), PR_TRUE, 0, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;
    PRBool exists;
    rv = file->Exists(&exists);
    if (NS_FAILED(rv) || !exists) return NS_ERROR_NOT_AVAILABLE;

    nsCacheEntry* entry;
    rv = NS_NewCacheEntry(&entry, key, PR_TRUE, nsICache::STORE_ON_DISK, this);
    if (NS_FAILED(rv)) return rv;
    
    do {
        DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
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
        rv = transport->OpenInputStream(0, -1, 0, getter_AddRefs(input));
        if (NS_FAILED(rv)) break;
        
        // read the metadata file.
        MetaDataFile metaDataFile;
        rv = metaDataFile.Read(input);
        input->Close();
        if (NS_FAILED(rv)) break;
        
        // Ensure that the keys match.
        if (nsCRT::strcmp(key->get(), metaDataFile.mKey) != 0) break;
        
        // initialize the entry.
        entry->SetFetchCount(metaDataFile.mFetchCount);
        entry->SetLastFetched(metaDataFile.mLastFetched);
        entry->SetLastValidated(metaDataFile.mLastValidated);
        entry->SetLastValidated(metaDataFile.mLastValidated);
        entry->SetExpirationTime(metaDataFile.mExpirationTime);
        entry->SetDataSize(metaDataFile.mDataSize);
        
        // restore the metadata.
        if (metaDataFile.mMetaDataSize) {
            rv = entry->UnflattenMetaData(metaDataFile.mMetaData, metaDataFile.mMetaDataSize);
            if (NS_FAILED(rv)) break;
        }
        
        // celebrate!        
        *result = entry;
        return NS_OK;
    } while (0);

    // oh, auto_ptr<> would be nice right about now.    
    delete entry;
    return rv;
}

nsresult nsDiskCacheDevice::deleteDiskCacheEntry(DiskCacheEntry * diskEntry)
{
    nsresult rv;
    
    // delete the meta file.
    nsCOMPtr<nsIFile> metaFile;
    rv = getFileForDiskCacheEntry(diskEntry, PR_TRUE,
                                  getter_AddRefs(metaFile));
    if (NS_SUCCEEDED(rv)) {
        rv = metaFile->Delete(PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsDiskCacheDevice::deleteDiskCacheEntry");
    }
    
    // delete the data file
    nsCOMPtr<nsIFile> dataFile;
    rv = getFileForDiskCacheEntry(diskEntry, PR_FALSE,
                                  getter_AddRefs(dataFile));
    if (NS_SUCCEEDED(rv)) {
        rv = dataFile->Delete(PR_FALSE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "nsDiskCacheDevice::deleteDiskCacheEntry");
    }
    
    return NS_OK;
}

/**
 * This can be sped up by keeping a secondary hash table that lets us look up
 * bound entries by their on-disk hash number. An STL map would be handy
 * for this. For now, just use brute force.
 */
 
struct FindCollisionVisitor : public nsCacheEntryHashTable::Visitor {
    PLDHashNumber mEntryHash;
    nsCacheEntry* mCollidedEntry;

    FindCollisionVisitor(nsCacheEntry * entry) : mEntryHash(0), mCollidedEntry(nsnull)
    {
        mEntryHash = ::PL_DHashStringKey(NULL, entry->Key()->get());
    }
    
    virtual PRBool VisitEntry(nsCacheEntry * entry)
    {
        if (mEntryHash == ::PL_DHashStringKey(NULL, entry->Key()->get())) {
            mCollidedEntry = entry;
            return PR_FALSE;
        }
        return PR_TRUE;
    }
};

nsresult nsDiskCacheDevice::checkForCollision(nsCacheEntry * entry, nsCacheEntry ** collidingEntry)
{
    FindCollisionVisitor visitor(entry);
    mBoundEntries.VisitEntries(&visitor);
    *collidingEntry = visitor.mCollidedEntry;
    return NS_OK;

#if 0
    // first, check if a metadata file exists for this entry already.
    nsresult rv;
    nsCOMPtr<nsIFile> file;
    nsCString* key = entry->Key();
    rv = getFileForKey(key->get(), PR_TRUE, getter_AddRefs(file));
    if (NS_FAILED(rv)) return rv;

    PRBool exists;
    rv = file->Exists(&exists);
    if (NS_FAILED(rv) || !exists) return NS_OK;

    DiskCacheEntry* diskEntry = ensureDiskCacheEntry(entry);
    if (!diskEntry) return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsITransport>& transport = diskEntry->getMetaTransport(nsICache::ACCESS_READ);
    if (!transport) {
        rv = getTransportForFile(file, nsICache::ACCESS_READ, getter_AddRefs(transport));
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIInputStream> input;
    rv = transport->OpenInputStream(0, -1, 0, getter_AddRefs(input));
    if (NS_FAILED(rv)) return rv;
    
    // read the metadata file.
    MetaDataFile metaDataFile;
    rv = metaDataFile.Read(input);
    input->Close();
    if (NS_FAILED(rv)) return rv;

    // Ensure that the keys don't match.
    if (nsCRT::strcmp(key->get(), metaDataFile.mKey) == 0) return NS_OK;
    
    nsSubsumeCStr collidingKey(metaDataFile.mKey, PR_FALSE);
    *collidingEntry = mBoundEntries.GetEntry(&collidingKey);
    return NS_OK;
#endif
}

nsresult nsDiskCacheDevice::scavengeDiskCacheEntries(DiskCacheEntry * diskEntry)
{
    nsresult rv;
    
    // count the number of doomed entries still in the list, not
    // including the passed-in entry. if this number is zero, then
    // the liveEntry, if inactive, can have its generation reset
    // to zero.
    PRUint32 doomedEntryCount = 0;
    DiskCacheEntry * youngestDiskEntry = diskEntry;
    nsCacheEntry * youngestEntry = diskEntry->getCacheEntry();
    DiskCacheEntry * nextDiskEntry = NS_STATIC_CAST(DiskCacheEntry*, PR_NEXT_LINK(diskEntry));
    while (nextDiskEntry != diskEntry) {
        nsCacheEntry* nextEntry = nextDiskEntry->getCacheEntry();
        if (nextEntry->IsDoomed()) {
            ++doomedEntryCount;
        } else if (nextDiskEntry->getGeneration() > youngestDiskEntry->getGeneration()) {
            youngestEntry = nextEntry;
            youngestDiskEntry = nextDiskEntry;
        }
        nextDiskEntry = NS_STATIC_CAST(DiskCacheEntry*, PR_NEXT_LINK(nextDiskEntry));
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
