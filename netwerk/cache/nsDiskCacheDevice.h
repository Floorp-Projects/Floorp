/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheDevice_h_
#define _nsDiskCacheDevice_h_

#include "mozilla/MemoryReporting.h"
#include "nsCacheDevice.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheBlockFile.h"
#include "nsDiskCacheEntry.h"

#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsCOMArray.h"

class nsDiskCacheMap;
class nsIMemoryReporter;


class nsDiskCacheDevice : public nsCacheDevice {
public:
    nsDiskCacheDevice();
    virtual ~nsDiskCacheDevice();

    virtual nsresult        Init();
    virtual nsresult        Shutdown();

    virtual const char *    GetDeviceID(void);
    virtual nsCacheEntry *  FindEntry(nsCString * key, bool *collision);
    virtual nsresult        DeactivateEntry(nsCacheEntry * entry);
    virtual nsresult        BindEntry(nsCacheEntry * entry);
    virtual void            DoomEntry( nsCacheEntry * entry );

    virtual nsresult OpenInputStreamForEntry(nsCacheEntry *    entry,
                                             nsCacheAccessMode mode,
                                             uint32_t          offset,
                                             nsIInputStream ** result);

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              uint32_t           offset,
                                              nsIOutputStream ** result);

    virtual nsresult        GetFileForEntry(nsCacheEntry *    entry,
                                            nsIFile **        result);

    virtual nsresult        OnDataSizeChange(nsCacheEntry * entry, int32_t deltaSize);
    
    virtual nsresult        Visit(nsICacheVisitor * visitor);

    virtual nsresult        EvictEntries(const char * clientID);

    bool                    EntryIsTooBig(int64_t entrySize);

    size_t                 SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

    /**
     * Preference accessors
     */
    void                    SetCacheParentDirectory(nsIFile * parentDir);
    void                    SetCapacity(uint32_t  capacity);
    void                    SetMaxEntrySize(int32_t  maxSizeInKilobytes);

/* private: */

    void                    getCacheDirectory(nsIFile ** result);
    uint32_t                getCacheCapacity();
    uint32_t                getCacheSize();
    uint32_t                getEntryCount();
    
    nsDiskCacheMap *        CacheMap()    { return &mCacheMap; }
    
private:    
    friend class nsDiskCacheDeviceDeactivateEntryEvent;
    friend class nsEvictDiskCacheEntriesEvent;
    friend class nsDiskCacheMap;
    /**
     *  Private methods
     */

    inline bool IsValidBinding(nsDiskCacheBinding *binding)
    {
        NS_ASSERTION(binding, "  binding == nullptr");
        NS_ASSERTION(binding->mDeactivateEvent == nullptr,
                     "  entry in process of deactivation");
        return (binding && !binding->mDeactivateEvent);
    }

    bool                    Initialized() { return mInitialized; }

    nsresult                Shutdown_Private(bool flush);
    nsresult                DeactivateEntry_Private(nsCacheEntry * entry,
                                                    nsDiskCacheBinding * binding);

    nsresult                OpenDiskCache();
    nsresult                ClearDiskCache();

    nsresult                EvictDiskCacheEntries(uint32_t  targetCapacity);
    
    /**
     *  Member variables
     */
    nsCOMPtr<nsIFile>       mCacheDirectory;
    nsDiskCacheBindery      mBindery;
    uint32_t                mCacheCapacity;     // Unit is KiB's
    int32_t                 mMaxEntrySize;      // Unit is bytes internally
    // XXX need soft/hard limits, currentTotal
    nsDiskCacheMap          mCacheMap;
    bool                    mInitialized;
    bool                    mClearingDiskCache;

    nsCOMPtr<nsIMemoryReporter> mReporter;
};

#endif // _nsDiskCacheDevice_h_
