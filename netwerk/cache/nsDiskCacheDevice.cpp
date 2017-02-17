/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>

#include "mozilla/DebugOnly.h"

#include "nsCache.h"
#include "nsIMemoryReporter.h"

// include files for ftruncate (or equivalent)
#if defined(XP_UNIX)
#include <unistd.h>
#elif defined(XP_WIN)
#include <windows.h>
#else
// XXX add necessary include file for ftruncate (or equivalent)
#endif

#include "prthread.h"

#include "private/pprio.h"

#include "nsDiskCacheDevice.h"
#include "nsDiskCacheEntry.h"
#include "nsDiskCacheMap.h"
#include "nsDiskCacheStreams.h"

#include "nsDiskCache.h"

#include "nsCacheService.h"

#include "nsDeleteDir.h"

#include "nsICacheVisitor.h"
#include "nsReadableUtils.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCRT.h"
#include "nsCOMArray.h"
#include "nsISimpleEnumerator.h"

#include "nsThreadUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Telemetry.h"

static const char DISK_CACHE_DEVICE_ID[] = { "disk" };
using namespace mozilla;

class nsDiskCacheDeviceDeactivateEntryEvent : public Runnable {
public:
    nsDiskCacheDeviceDeactivateEntryEvent(nsDiskCacheDevice *device,
                                          nsCacheEntry * entry,
                                          nsDiskCacheBinding * binding)
        : mCanceled(false),
          mEntry(entry),
          mDevice(device),
          mBinding(binding)
    {
    }

    NS_IMETHOD Run() override
    {
        nsCacheServiceAutoLock lock;
        CACHE_LOG_DEBUG(("nsDiskCacheDeviceDeactivateEntryEvent[%p]\n", this));
        if (!mCanceled) {
            (void) mDevice->DeactivateEntry_Private(mEntry, mBinding);
        }
        return NS_OK;
    }

    void CancelEvent() { mCanceled = true; }
private:
    bool mCanceled;
    nsCacheEntry *mEntry;
    nsDiskCacheDevice *mDevice;
    nsDiskCacheBinding *mBinding;
};

class nsEvictDiskCacheEntriesEvent : public Runnable {
public:
    explicit nsEvictDiskCacheEntriesEvent(nsDiskCacheDevice *device)
        : mDevice(device) {}

    NS_IMETHOD Run() override
    {
        nsCacheServiceAutoLock lock;
        mDevice->EvictDiskCacheEntries(mDevice->mCacheCapacity);
        return NS_OK;
    }

private:
    nsDiskCacheDevice *mDevice;
};

/******************************************************************************
 *  nsDiskCacheEvictor
 *
 *  Helper class for nsDiskCacheDevice.
 *
 *****************************************************************************/

class nsDiskCacheEvictor : public nsDiskCacheRecordVisitor
{
public:
    nsDiskCacheEvictor( nsDiskCacheMap *      cacheMap,
                        nsDiskCacheBindery *  cacheBindery,
                        uint32_t              targetSize,
                        const char *          clientID)
        : mCacheMap(cacheMap)
        , mBindery(cacheBindery)
        , mTargetSize(targetSize)
        , mClientID(clientID)
    { 
        mClientIDSize = clientID ? strlen(clientID) : 0;
    }
    
    virtual int32_t  VisitRecord(nsDiskCacheRecord *  mapRecord);
 
private:
        nsDiskCacheMap *     mCacheMap;
        nsDiskCacheBindery * mBindery;
        uint32_t             mTargetSize;
        const char *         mClientID;
        uint32_t             mClientIDSize;
};


int32_t
nsDiskCacheEvictor::VisitRecord(nsDiskCacheRecord *  mapRecord)
{
    if (mCacheMap->TotalSize() < mTargetSize)
        return kStopVisitingRecords;
    
    if (mClientID) {
        // we're just evicting records for a specific client
        nsDiskCacheEntry * diskEntry = mCacheMap->ReadDiskCacheEntry(mapRecord);
        if (!diskEntry)
            return kVisitNextRecord;  // XXX or delete record?
    
        // Compare clientID's without malloc
        if ((diskEntry->mKeySize <= mClientIDSize) ||
            (diskEntry->Key()[mClientIDSize] != ':') ||
            (memcmp(diskEntry->Key(), mClientID, mClientIDSize) != 0)) {
            return kVisitNextRecord;  // clientID doesn't match, skip it
        }
    }
    
    nsDiskCacheBinding * binding = mBindery->FindActiveBinding(mapRecord->HashNumber());
    if (binding) {
        // If the entry is pending deactivation, cancel deactivation and doom
        // the entry
        if (binding->mDeactivateEvent) {
            binding->mDeactivateEvent->CancelEvent();
            binding->mDeactivateEvent = nullptr;
        }
        // We are currently using this entry, so all we can do is doom it.
        // Since we're enumerating the records, we don't want to call
        // DeleteRecord when nsCacheService::DoomEntry() calls us back.
        binding->mDoomed = true;         // mark binding record as 'deleted'
        nsCacheService::DoomEntry(binding->mCacheEntry);
    } else {
        // entry not in use, just delete storage because we're enumerating the records
        (void) mCacheMap->DeleteStorage(mapRecord);
    }

    return kDeleteRecordAndContinue;  // this will REALLY delete the record
}


/******************************************************************************
 *  nsDiskCacheDeviceInfo
 *****************************************************************************/

class nsDiskCacheDeviceInfo : public nsICacheDeviceInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEDEVICEINFO

    explicit nsDiskCacheDeviceInfo(nsDiskCacheDevice* device)
        :   mDevice(device)
    {
    }

private:
    virtual ~nsDiskCacheDeviceInfo() {}

    nsDiskCacheDevice* mDevice;
};

NS_IMPL_ISUPPORTS(nsDiskCacheDeviceInfo, nsICacheDeviceInfo)

NS_IMETHODIMP nsDiskCacheDeviceInfo::GetDescription(char ** aDescription)
{
    NS_ENSURE_ARG_POINTER(aDescription);
    *aDescription = NS_strdup("Disk cache device");
    return *aDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsDiskCacheDeviceInfo::GetUsageReport(char ** usageReport)
{
    NS_ENSURE_ARG_POINTER(usageReport);
    nsCString buffer;
    
    buffer.AssignLiteral("  <tr>\n"
                         "    <th>Cache Directory:</th>\n"
                         "    <td>");
    nsCOMPtr<nsIFile> cacheDir;
    nsAutoString path;
    mDevice->getCacheDirectory(getter_AddRefs(cacheDir)); 
    nsresult rv = cacheDir->GetPath(path);
    if (NS_SUCCEEDED(rv)) {
        AppendUTF16toUTF8(path, buffer);
    } else {
        buffer.AppendLiteral("directory unavailable");
    }
    buffer.AppendLiteral("</td>\n"
                         "  </tr>\n");

    *usageReport = ToNewCString(buffer);
    if (!*usageReport) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheDeviceInfo::GetEntryCount(uint32_t *aEntryCount)
{
    NS_ENSURE_ARG_POINTER(aEntryCount);
    *aEntryCount = mDevice->getEntryCount();
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheDeviceInfo::GetTotalSize(uint32_t *aTotalSize)
{
    NS_ENSURE_ARG_POINTER(aTotalSize);
    // Returned unit's are in bytes
    *aTotalSize = mDevice->getCacheSize() * 1024;
    return NS_OK;
}

NS_IMETHODIMP nsDiskCacheDeviceInfo::GetMaximumSize(uint32_t *aMaximumSize)
{
    NS_ENSURE_ARG_POINTER(aMaximumSize);
    // Returned unit's are in bytes
    *aMaximumSize = mDevice->getCacheCapacity() * 1024;
    return NS_OK;
}


/******************************************************************************
 *  nsDiskCache
 *****************************************************************************/

/**
 *  nsDiskCache::Hash(const char * key, PLDHashNumber initval)
 *
 *  See http://burtleburtle.net/bob/hash/evahash.html for more information
 *  about this hash function.
 *
 *  This algorithm of this method implies nsDiskCacheRecords will be stored
 *  in a certain order on disk.  If the algorithm changes, existing cache
 *  map files may become invalid, and therefore the kCurrentVersion needs
 *  to be revised.
 */

static inline void hashmix(uint32_t& a, uint32_t& b, uint32_t& c)
{
  a -= b; a -= c; a ^= (c>>13);
  b -= c; b -= a; b ^= (a<<8);
  c -= a; c -= b; c ^= (b>>13);
  a -= b; a -= c; a ^= (c>>12); 
  b -= c; b -= a; b ^= (a<<16);
  c -= a; c -= b; c ^= (b>>5);
  a -= b; a -= c; a ^= (c>>3);
  b -= c; b -= a; b ^= (a<<10);
  c -= a; c -= b; c ^= (b>>15);
}

PLDHashNumber
nsDiskCache::Hash(const char * key, PLDHashNumber initval)
{
  const uint8_t *k = reinterpret_cast<const uint8_t*>(key);
  uint32_t a, b, c, len, length;

  length = strlen(key);
  /* Set up the internal state */
  len = length;
  a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  c = initval;         /* variable initialization of internal state */

  /*---------------------------------------- handle most of the key */
  while (len >= 12)
  {
    a += k[0] + (uint32_t(k[1])<<8) + (uint32_t(k[2])<<16) + (uint32_t(k[3])<<24);
    b += k[4] + (uint32_t(k[5])<<8) + (uint32_t(k[6])<<16) + (uint32_t(k[7])<<24);
    c += k[8] + (uint32_t(k[9])<<8) + (uint32_t(k[10])<<16) + (uint32_t(k[11])<<24);
    hashmix(a, b, c);
    k += 12; len -= 12;
  }

  /*------------------------------------- handle the last 11 bytes */
  c += length;
  switch(len) {              /* all the case statements fall through */
    case 11: c += (uint32_t(k[10])<<24);  MOZ_FALLTHROUGH;
    case 10: c += (uint32_t(k[9])<<16);   MOZ_FALLTHROUGH;
    case 9 : c += (uint32_t(k[8])<<8);    MOZ_FALLTHROUGH;
    /* the low-order byte of c is reserved for the length */
    case 8 : b += (uint32_t(k[7])<<24);   MOZ_FALLTHROUGH;
    case 7 : b += (uint32_t(k[6])<<16);   MOZ_FALLTHROUGH;
    case 6 : b += (uint32_t(k[5])<<8);    MOZ_FALLTHROUGH;
    case 5 : b += k[4];                   MOZ_FALLTHROUGH;
    case 4 : a += (uint32_t(k[3])<<24);   MOZ_FALLTHROUGH;
    case 3 : a += (uint32_t(k[2])<<16);   MOZ_FALLTHROUGH;
    case 2 : a += (uint32_t(k[1])<<8);    MOZ_FALLTHROUGH;
    case 1 : a += k[0];
    /* case 0: nothing left to add */
  }
  hashmix(a, b, c);

  return c;
}

nsresult
nsDiskCache::Truncate(PRFileDesc *  fd, uint32_t  newEOF)
{
    // use modified SetEOF from nsFileStreams::SetEOF()

#if defined(XP_UNIX)
    if (ftruncate(PR_FileDesc2NativeHandle(fd), newEOF) != 0) {
        NS_ERROR("ftruncate failed");
        return NS_ERROR_FAILURE;
    }

#elif defined(XP_WIN)
    int32_t cnt = PR_Seek(fd, newEOF, PR_SEEK_SET);
    if (cnt == -1)  return NS_ERROR_FAILURE;
    if (!SetEndOfFile((HANDLE) PR_FileDesc2NativeHandle(fd))) {
        NS_ERROR("SetEndOfFile failed");
        return NS_ERROR_FAILURE;
    }

#else
    // add implementations for other platforms here
#endif
    return NS_OK;
}


/******************************************************************************
 *  nsDiskCacheDevice
 *****************************************************************************/

nsDiskCacheDevice::nsDiskCacheDevice()
    : mCacheCapacity(0)
    , mMaxEntrySize(-1) // -1 means "no limit"
    , mInitialized(false)
    , mClearingDiskCache(false)
{
}

nsDiskCacheDevice::~nsDiskCacheDevice()
{
    Shutdown();
}


/**
 *  methods of nsCacheDevice
 */
nsresult
nsDiskCacheDevice::Init()
{
    nsresult rv;

    if (Initialized()) {
        NS_ERROR("Disk cache already initialized!");
        return NS_ERROR_UNEXPECTED;
    }

    if (!mCacheDirectory)
        return NS_ERROR_FAILURE;

    mBindery.Init();

    // Open Disk Cache
    rv = OpenDiskCache();
    if (NS_FAILED(rv)) {
        (void) mCacheMap.Close(false);
        return rv;
    }

    mInitialized = true;
    return NS_OK;
}


/**
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::Shutdown()
{
    nsCacheService::AssertOwnsLock();

    nsresult rv = Shutdown_Private(true);
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
}


nsresult
nsDiskCacheDevice::Shutdown_Private(bool    flush)
{
    CACHE_LOG_DEBUG(("CACHE: disk Shutdown_Private [%u]\n", flush));

    if (Initialized()) {
        // check cache limits in case we need to evict.
        EvictDiskCacheEntries(mCacheCapacity);

        // At this point there may be a number of pending cache-requests on the
        // cache-io thread. Wait for all these to run before we wipe out our
        // datastructures (see bug #620660)
        (void) nsCacheService::SyncWithCacheIOThread();

        // write out persistent information about the cache.
        (void) mCacheMap.Close(flush);

        mBindery.Reset();

        mInitialized = false;
    }

    return NS_OK;
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
 *
 *  NOTE: called while holding the cache service lock
 */
nsCacheEntry *
nsDiskCacheDevice::FindEntry(nsCString * key, bool *collision)
{
    Telemetry::AutoTimer<Telemetry::CACHE_DISK_SEARCH_2> timer;
    if (!Initialized())  return nullptr;  // NS_ERROR_NOT_INITIALIZED
    if (mClearingDiskCache)  return nullptr;
    nsDiskCacheRecord       record;
    nsDiskCacheBinding *    binding = nullptr;
    PLDHashNumber           hashNumber = nsDiskCache::Hash(key->get());

    *collision = false;

    binding = mBindery.FindActiveBinding(hashNumber);
    if (binding && !binding->mCacheEntry->Key()->Equals(*key)) {
        *collision = true;
        return nullptr;
    } else if (binding && binding->mDeactivateEvent) {
        binding->mDeactivateEvent->CancelEvent();
        binding->mDeactivateEvent = nullptr;
        CACHE_LOG_DEBUG(("CACHE: reusing deactivated entry %p " \
                         "req-key=%s  entry-key=%s\n",
                         binding->mCacheEntry, key->get(),
                         binding->mCacheEntry->Key()->get()));

        return binding->mCacheEntry; // just return this one, observing that
                                     // FindActiveBinding() does not return
                                     // bindings to doomed entries
    }
    binding = nullptr;

    // lookup hash number in cache map
    nsresult rv = mCacheMap.FindRecord(hashNumber, &record);
    if (NS_FAILED(rv))  return nullptr;  // XXX log error?
    
    nsDiskCacheEntry * diskEntry = mCacheMap.ReadDiskCacheEntry(&record);
    if (!diskEntry) return nullptr;
    
    // compare key to be sure
    if (!key->Equals(diskEntry->Key())) {
        *collision = true;
        return nullptr;
    }
    
    nsCacheEntry * entry = diskEntry->CreateCacheEntry(this);
    if (entry) {
        binding = mBindery.CreateBinding(entry, &record);
        if (!binding) {
            delete entry;
            entry = nullptr;
        }
    }

    if (!entry) {
      (void) mCacheMap.DeleteStorage(&record);
      (void) mCacheMap.DeleteRecord(&record);
    }
    
    return entry;
}


/**
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    if (!IsValidBinding(binding))
        return NS_ERROR_UNEXPECTED;

    CACHE_LOG_DEBUG(("CACHE: disk DeactivateEntry [%p %x]\n",
        entry, binding->mRecord.HashNumber()));

    nsDiskCacheDeviceDeactivateEntryEvent *event =
        new nsDiskCacheDeviceDeactivateEntryEvent(this, entry, binding);

    // ensure we can cancel the event via the binding later if necessary
    binding->mDeactivateEvent = event;

    DebugOnly<nsresult> rv = nsCacheService::DispatchToCacheIOThread(event);
    NS_ASSERTION(NS_SUCCEEDED(rv), "DeactivateEntry: Failed dispatching "
                                   "deactivation event");
    return NS_OK;
}

/**
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::DeactivateEntry_Private(nsCacheEntry * entry,
                                           nsDiskCacheBinding * binding)
{
    nsresult rv = NS_OK;
    if (entry->IsDoomed()) {
        // delete data, entry, record from disk for entry
        rv = mCacheMap.DeleteStorage(&binding->mRecord);

    } else {
        // save stuff to disk for entry
        rv = mCacheMap.WriteDiskCacheEntry(binding);
        if (NS_FAILED(rv)) {
            // clean up as best we can
            (void) mCacheMap.DeleteStorage(&binding->mRecord);
            (void) mCacheMap.DeleteRecord(&binding->mRecord);
            binding->mDoomed = true; // record is no longer in cache map
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
 *
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::BindEntry(nsCacheEntry * entry)
{
    if (!Initialized())  return  NS_ERROR_NOT_INITIALIZED;
    if (mClearingDiskCache)  return NS_ERROR_NOT_AVAILABLE;
    nsresult rv = NS_OK;
    nsDiskCacheRecord record, oldRecord;
    nsDiskCacheBinding *binding;
    PLDHashNumber hashNumber = nsDiskCache::Hash(entry->Key()->get());

    // Find out if there is already an active binding for this hash. If yes it
    // should have another key since BindEntry() shouldn't be called twice for
    // the same entry. Doom the old entry, the new one will get another
    // generation number so files won't collide.
    binding = mBindery.FindActiveBinding(hashNumber);
    if (binding) {
        NS_ASSERTION(!binding->mCacheEntry->Key()->Equals(*entry->Key()),
                     "BindEntry called for already bound entry!");
        // If the entry is pending deactivation, cancel deactivation
        if (binding->mDeactivateEvent) {
            binding->mDeactivateEvent->CancelEvent();
            binding->mDeactivateEvent = nullptr;
        }
        nsCacheService::DoomEntry(binding->mCacheEntry);
        binding = nullptr;
    }

    // Lookup hash number in cache map. There can be a colliding inactive entry.
    // See bug #321361 comment 21 for the scenario. If there is such entry,
    // delete it.
    rv = mCacheMap.FindRecord(hashNumber, &record);
    if (NS_SUCCEEDED(rv)) {
        nsDiskCacheEntry * diskEntry = mCacheMap.ReadDiskCacheEntry(&record);
        if (diskEntry) {
            // compare key to be sure
            if (!entry->Key()->Equals(diskEntry->Key())) {
                mCacheMap.DeleteStorage(&record);
                rv = mCacheMap.DeleteRecord(&record);
                if (NS_FAILED(rv))  return rv;
            }
        }
        record = nsDiskCacheRecord();
    }

    // create a new record for this entry
    record.SetHashNumber(nsDiskCache::Hash(entry->Key()->get()));
    record.SetEvictionRank(ULONG_MAX - SecondsFromPRTime(PR_Now()));

    CACHE_LOG_DEBUG(("CACHE: disk BindEntry [%p %x]\n",
        entry, record.HashNumber()));

    if (!entry->IsDoomed()) {
        // if entry isn't doomed, add it to the cache map
        rv = mCacheMap.AddRecord(&record, &oldRecord); // deletes old record, if any
        if (NS_FAILED(rv))  return rv;
        
        uint32_t    oldHashNumber = oldRecord.HashNumber();
        if (oldHashNumber) {
            // gotta evict this one first
            nsDiskCacheBinding * oldBinding = mBindery.FindActiveBinding(oldHashNumber);
            if (oldBinding) {
                // XXX if debug : compare keys for hashNumber collision

                if (!oldBinding->mCacheEntry->IsDoomed()) {
                    // If the old entry is pending deactivation, cancel deactivation
                    if (oldBinding->mDeactivateEvent) {
                        oldBinding->mDeactivateEvent->CancelEvent();
                        oldBinding->mDeactivateEvent = nullptr;
                    }
                // we've got a live one!
                    nsCacheService::DoomEntry(oldBinding->mCacheEntry);
                    // storage will be delete when oldBinding->mCacheEntry is Deactivated
                }
            } else {
                // delete storage
                // XXX if debug : compare keys for hashNumber collision
                rv = mCacheMap.DeleteStorage(&oldRecord);
                if (NS_FAILED(rv))  return rv;  // XXX delete record we just added?
            }
        }
    }
    
    // Make sure this entry has its associated nsDiskCacheBinding attached.
    binding = mBindery.CreateBinding(entry, &record);
    NS_ASSERTION(binding, "nsDiskCacheDevice::BindEntry");
    if (!binding) return NS_ERROR_OUT_OF_MEMORY;
    NS_ASSERTION(binding->mRecord.ValidRecord(), "bad cache map record");

    return NS_OK;
}


/**
 *  NOTE: called while holding the cache service lock
 */
void
nsDiskCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    CACHE_LOG_DEBUG(("CACHE: disk DoomEntry [%p]\n", entry));

    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    NS_ASSERTION(binding, "DoomEntry: binding == nullptr");
    if (!binding)
        return;

    if (!binding->mDoomed) {
        // so it can't be seen by FindEntry() ever again.
#ifdef DEBUG
        nsresult rv =
#endif
            mCacheMap.DeleteRecord(&binding->mRecord);
        NS_ASSERTION(NS_SUCCEEDED(rv),"DeleteRecord failed.");
        binding->mDoomed = true; // record in no longer in cache map
    }
}


/**
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::OpenInputStreamForEntry(nsCacheEntry *      entry,
                                           nsCacheAccessMode   mode, 
                                           uint32_t            offset,
                                           nsIInputStream **   result)
{
    CACHE_LOG_DEBUG(("CACHE: disk OpenInputStreamForEntry [%p %x %u]\n",
        entry, mode, offset));

    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    nsresult             rv;
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    if (!IsValidBinding(binding))
        return NS_ERROR_UNEXPECTED;

    NS_ASSERTION(binding->mCacheEntry == entry, "binding & entry don't point to each other");

    rv = binding->EnsureStreamIO();
    if (NS_FAILED(rv)) return rv;

    return binding->mStreamIO->GetInputStream(offset, result);
}


/**
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::OpenOutputStreamForEntry(nsCacheEntry *      entry,
                                            nsCacheAccessMode   mode, 
                                            uint32_t            offset,
                                            nsIOutputStream **  result)
{
    CACHE_LOG_DEBUG(("CACHE: disk OpenOutputStreamForEntry [%p %x %u]\n",
        entry, mode, offset));
 
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    nsresult             rv;
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    if (!IsValidBinding(binding))
        return NS_ERROR_UNEXPECTED;
    
    NS_ASSERTION(binding->mCacheEntry == entry, "binding & entry don't point to each other");

    rv = binding->EnsureStreamIO();
    if (NS_FAILED(rv)) return rv;

    return binding->mStreamIO->GetOutputStream(offset, result);
}


/**
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::GetFileForEntry(nsCacheEntry *    entry,
                                   nsIFile **        result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = nullptr;

    nsresult             rv;
        
    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    if (!IsValidBinding(binding))
        return NS_ERROR_UNEXPECTED;

    // check/set binding->mRecord for separate file, sync w/mCacheMap
    if (binding->mRecord.DataLocationInitialized()) {
        if (binding->mRecord.DataFile() != 0)
            return NS_ERROR_NOT_AVAILABLE;  // data not stored as separate file

        NS_ASSERTION(binding->mRecord.DataFileGeneration() == binding->mGeneration, "error generations out of sync");
    } else {
        binding->mRecord.SetDataFileGeneration(binding->mGeneration);
        binding->mRecord.SetDataFileSize(0);    // 1k minimum
        if (!binding->mDoomed) {
            // record stored in cache map, so update it
            rv = mCacheMap.UpdateRecord(&binding->mRecord);
            if (NS_FAILED(rv))  return rv;
        }
    }
    
    nsCOMPtr<nsIFile>  file;
    rv = mCacheMap.GetFileForDiskCacheRecord(&binding->mRecord,
                                             nsDiskCache::kData,
                                             false,
                                             getter_AddRefs(file));
    if (NS_FAILED(rv))  return rv;
    
    NS_IF_ADDREF(*result = file);
    return NS_OK;
}


/**
 *  This routine will get called every time an open descriptor is written to.
 *
 *  NOTE: called while holding the cache service lock
 */
nsresult
nsDiskCacheDevice::OnDataSizeChange(nsCacheEntry * entry, int32_t deltaSize)
{
    CACHE_LOG_DEBUG(("CACHE: disk OnDataSizeChange [%p %d]\n",
        entry, deltaSize));

    // If passed a negative value, then there's nothing to do.
    if (deltaSize < 0)
        return NS_OK;

    nsDiskCacheBinding * binding = GetCacheEntryBinding(entry);
    if (!IsValidBinding(binding))
        return NS_ERROR_UNEXPECTED;

    NS_ASSERTION(binding->mRecord.ValidRecord(), "bad record");

    uint32_t  newSize = entry->DataSize() + deltaSize;
    uint32_t  newSizeK =  ((newSize + 0x3FF) >> 10);

    // If the new size is larger than max. file size or larger than
    // 1/8 the cache capacity (which is in KiB's), doom the entry and abort.
    if (EntryIsTooBig(newSize)) {
#ifdef DEBUG
        nsresult rv =
#endif
            nsCacheService::DoomEntry(entry);
        NS_ASSERTION(NS_SUCCEEDED(rv),"DoomEntry() failed.");
        return NS_ERROR_ABORT;
    }

    uint32_t  sizeK = ((entry->DataSize() + 0x03FF) >> 10); // round up to next 1k

    // In total count we ignore anything over kMaxDataSizeK (bug #651100), so
    // the target capacity should be calculated the same way.
    if (sizeK > kMaxDataSizeK) sizeK = kMaxDataSizeK;
    if (newSizeK > kMaxDataSizeK) newSizeK = kMaxDataSizeK;

    // pre-evict entries to make space for new data
    uint32_t  targetCapacity = mCacheCapacity > (newSizeK - sizeK)
                             ? mCacheCapacity - (newSizeK - sizeK)
                             : 0;
    EvictDiskCacheEntries(targetCapacity);
    
    return NS_OK;
}


/******************************************************************************
 *  EntryInfoVisitor
 *****************************************************************************/
class EntryInfoVisitor : public nsDiskCacheRecordVisitor
{
public:
    EntryInfoVisitor(nsDiskCacheMap *    cacheMap,
                     nsICacheVisitor *   visitor)
        : mCacheMap(cacheMap)
        , mVisitor(visitor)
    {}
    
    virtual int32_t  VisitRecord(nsDiskCacheRecord *  mapRecord)
    {
        // XXX optimization: do we have this record in memory?
        
        // read in the entry (metadata)
        nsDiskCacheEntry * diskEntry = mCacheMap->ReadDiskCacheEntry(mapRecord);
        if (!diskEntry) {
            return kVisitNextRecord;
        }

        // create nsICacheEntryInfo
        nsDiskCacheEntryInfo * entryInfo = new nsDiskCacheEntryInfo(DISK_CACHE_DEVICE_ID, diskEntry);
        if (!entryInfo) {
            return kStopVisitingRecords;
        }
        nsCOMPtr<nsICacheEntryInfo> ref(entryInfo);
        
        bool    keepGoing;
        (void)mVisitor->VisitEntry(DISK_CACHE_DEVICE_ID, entryInfo, &keepGoing);
        return keepGoing ? kVisitNextRecord : kStopVisitingRecords;
    }
 
private:
        nsDiskCacheMap *    mCacheMap;
        nsICacheVisitor *   mVisitor;
};


nsresult
nsDiskCacheDevice::Visit(nsICacheVisitor * visitor)
{
    if (!Initialized())  return NS_ERROR_NOT_INITIALIZED;
    nsDiskCacheDeviceInfo* deviceInfo = new nsDiskCacheDeviceInfo(this);
    nsCOMPtr<nsICacheDeviceInfo> ref(deviceInfo);
    
    bool keepGoing;
    nsresult rv = visitor->VisitDevice(DISK_CACHE_DEVICE_ID, deviceInfo, &keepGoing);
    if (NS_FAILED(rv)) return rv;
    
    if (keepGoing) {
        EntryInfoVisitor  infoVisitor(&mCacheMap, visitor);
        return mCacheMap.VisitRecords(&infoVisitor);
    }

    return NS_OK;
}

// Max allowed size for an entry is currently MIN(mMaxEntrySize, 1/8 CacheCapacity)
bool
nsDiskCacheDevice::EntryIsTooBig(int64_t entrySize)
{
    if (mMaxEntrySize == -1) // no limit
        return entrySize > (static_cast<int64_t>(mCacheCapacity) * 1024 / 8);
    else 
        return entrySize > mMaxEntrySize ||
               entrySize > (static_cast<int64_t>(mCacheCapacity) * 1024 / 8);
}

nsresult
nsDiskCacheDevice::EvictEntries(const char * clientID)
{
    CACHE_LOG_DEBUG(("CACHE: disk EvictEntries [%s]\n", clientID));

    if (!Initialized())  return NS_ERROR_NOT_INITIALIZED;
    nsresult  rv;

    if (clientID == nullptr) {
        // we're clearing the entire disk cache
        rv = ClearDiskCache();
        if (rv != NS_ERROR_CACHE_IN_USE)
            return rv;
    }

    nsDiskCacheEvictor  evictor(&mCacheMap, &mBindery, 0, clientID);
    rv = mCacheMap.VisitRecords(&evictor);
    
    if (clientID == nullptr)     // we tried to clear the entire cache
        rv = mCacheMap.Trim(); // so trim cache block files (if possible)
    return rv;
}


/**
 *  private methods
 */

nsresult
nsDiskCacheDevice::OpenDiskCache()
{
    Telemetry::AutoTimer<Telemetry::NETWORK_DISK_CACHE_OPEN> timer;
    // if we don't have a cache directory, create one and open it
    bool exists;
    nsresult rv = mCacheDirectory->Exists(&exists);
    if (NS_FAILED(rv))
        return rv;

    if (exists) {
        // Try opening cache map file.
        nsDiskCache::CorruptCacheInfo corruptInfo;
        rv = mCacheMap.Open(mCacheDirectory, &corruptInfo);

        if (rv == NS_ERROR_ALREADY_INITIALIZED) {
          NS_WARNING("nsDiskCacheDevice::OpenDiskCache: already open!");
        } else if (NS_FAILED(rv)) {
            // Consider cache corrupt: delete it
            // delay delete by 1 minute to avoid IO thrash at startup
            rv = nsDeleteDir::DeleteDir(mCacheDirectory, true, 60000);
            if (NS_FAILED(rv))
                return rv;
            exists = false;
        }
    }

    // if we don't have a cache directory, create one and open it
    if (!exists) {
        nsCacheService::MarkStartingFresh();
        rv = mCacheDirectory->Create(nsIFile::DIRECTORY_TYPE, 0777);
        CACHE_LOG_PATH(LogLevel::Info, "\ncreate cache directory: %s\n", mCacheDirectory);
        CACHE_LOG_INFO(("mCacheDirectory->Create() = %" PRIx32 "\n",
                        static_cast<uint32_t>(rv)));
        if (NS_FAILED(rv))
            return rv;
    
        // reopen the cache map     
        nsDiskCache::CorruptCacheInfo corruptInfo;
        rv = mCacheMap.Open(mCacheDirectory, &corruptInfo);
        if (NS_FAILED(rv))
            return rv;
    }

    return NS_OK;
}


nsresult
nsDiskCacheDevice::ClearDiskCache()
{
    if (mBindery.ActiveBindings())
        return NS_ERROR_CACHE_IN_USE;

    mClearingDiskCache = true;

    nsresult rv = Shutdown_Private(false);  // false: don't bother flushing
    if (NS_FAILED(rv))
        return rv;

    mClearingDiskCache = false;

    // If the disk cache directory is already gone, then it's not an error if
    // we fail to delete it ;-)
    rv = nsDeleteDir::DeleteDir(mCacheDirectory, true);
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST)
        return rv;

    return Init();
}


nsresult
nsDiskCacheDevice::EvictDiskCacheEntries(uint32_t  targetCapacity)
{
    CACHE_LOG_DEBUG(("CACHE: disk EvictDiskCacheEntries [%u]\n",
        targetCapacity));

    NS_ASSERTION(targetCapacity > 0, "oops");

    if (mCacheMap.TotalSize() < targetCapacity)
        return NS_OK;

    // targetCapacity is in KiB's
    nsDiskCacheEvictor  evictor(&mCacheMap, &mBindery, targetCapacity, nullptr);
    return mCacheMap.EvictRecords(&evictor);
}


/**
 *  methods for prefs
 */

void
nsDiskCacheDevice::SetCacheParentDirectory(nsIFile * parentDir)
{
    nsresult rv;
    bool    exists;

    if (Initialized()) {
        NS_ASSERTION(false, "Cannot switch cache directory when initialized");
        return;
    }

    if (!parentDir) {
        mCacheDirectory = nullptr;
        return;
    }

    // ensure parent directory exists
    rv = parentDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = parentDir->Create(nsIFile::DIRECTORY_TYPE, 0700);
    if (NS_FAILED(rv))  return;

    // ensure cache directory exists
    nsCOMPtr<nsIFile> directory;
    
    rv = parentDir->Clone(getter_AddRefs(directory));
    if (NS_FAILED(rv))  return;
    rv = directory->AppendNative(NS_LITERAL_CSTRING("Cache"));
    if (NS_FAILED(rv))  return;
    
    mCacheDirectory = do_QueryInterface(directory);
}


void
nsDiskCacheDevice::getCacheDirectory(nsIFile ** result)
{
    *result = mCacheDirectory;
    NS_IF_ADDREF(*result);
}


/**
 *  NOTE: called while holding the cache service lock
 */
void
nsDiskCacheDevice::SetCapacity(uint32_t  capacity)
{
    // Units are KiB's
    mCacheCapacity = capacity;
    if (Initialized()) {
        if (NS_IsMainThread()) {
            // Do not evict entries on the main thread
            nsCacheService::DispatchToCacheIOThread(
                new nsEvictDiskCacheEntriesEvent(this));
        } else {
            // start evicting entries if the new size is smaller!
            EvictDiskCacheEntries(mCacheCapacity);
        }
    }
    // Let cache map know of the new capacity
    mCacheMap.NotifyCapacityChange(capacity);
}


uint32_t nsDiskCacheDevice::getCacheCapacity()
{
    return mCacheCapacity;
}


uint32_t nsDiskCacheDevice::getCacheSize()
{
    return mCacheMap.TotalSize();
}


uint32_t nsDiskCacheDevice::getEntryCount()
{
    return mCacheMap.EntryCount();
}

void
nsDiskCacheDevice::SetMaxEntrySize(int32_t maxSizeInKilobytes)
{
    // Internal units are bytes. Changing this only takes effect *after* the
    // change and has no consequences for existing cache-entries
    if (maxSizeInKilobytes >= 0)
        mMaxEntrySize = maxSizeInKilobytes * 1024;
    else
        mMaxEntrySize = -1;
}

size_t
nsDiskCacheDevice::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
    size_t usage = aMallocSizeOf(this);

    usage += mCacheMap.SizeOfExcludingThis(aMallocSizeOf);
    usage += mBindery.SizeOfExcludingThis(aMallocSizeOf);

    return usage;
}
