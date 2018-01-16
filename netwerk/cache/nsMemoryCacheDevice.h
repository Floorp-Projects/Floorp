/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsMemoryCacheDevice_h_
#define _nsMemoryCacheDevice_h_

#include "nsCacheDevice.h"
#include "PLDHashTable.h"
#include "nsCacheEntry.h"


class nsMemoryCacheDeviceInfo;

/******************************************************************************
 * nsMemoryCacheDevice
 ******************************************************************************/
class nsMemoryCacheDevice : public nsCacheDevice
{
public:
    nsMemoryCacheDevice();
    virtual ~nsMemoryCacheDevice();

    virtual nsresult        Init() override;
    virtual nsresult        Shutdown() override;

    virtual const char *    GetDeviceID(void) override;

    virtual nsresult        BindEntry( nsCacheEntry * entry ) override;
    virtual nsCacheEntry *  FindEntry( nsCString * key, bool *collision ) override;
    virtual void            DoomEntry( nsCacheEntry * entry ) override;
    virtual nsresult        DeactivateEntry( nsCacheEntry * entry ) override;

    virtual nsresult OpenInputStreamForEntry(nsCacheEntry *     entry,
                                             nsCacheAccessMode  mode,
                                             uint32_t           offset,
                                             nsIInputStream **  result) override;

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              uint32_t           offset,
                                              nsIOutputStream ** result) override;

    virtual nsresult GetFileForEntry( nsCacheEntry *    entry,
                                      nsIFile **        result ) override;

    virtual nsresult OnDataSizeChange( nsCacheEntry * entry,
                                       int32_t deltaSize ) override;

    virtual nsresult Visit( nsICacheVisitor * visitor ) override;

    virtual nsresult EvictEntries(const char * clientID) override;
    nsresult EvictPrivateEntries();

    void             SetCapacity(int32_t  capacity);
    void             SetMaxEntrySize(int32_t  maxSizeInKilobytes);

    bool             EntryIsTooBig(int64_t entrySize);

    size_t           TotalSize();

private:
    friend class nsMemoryCacheDeviceInfo;
    enum      { DELETE_ENTRY        = true,
                DO_NOT_DELETE_ENTRY = false };

    void      AdjustMemoryLimits( int32_t  softLimit, int32_t  hardLimit);
    void      EvictEntry( nsCacheEntry * entry , bool deleteEntry);
    void      EvictEntriesIfNecessary();
    int       EvictionList(nsCacheEntry * entry, int32_t  deltaSize);

    typedef bool (*EvictionMatcherFn)(nsCacheEntry* entry, void* args);
    nsresult DoEvictEntries(EvictionMatcherFn matchFn, void* args);

#ifdef DEBUG
    void      CheckEntryCount();
#endif
    /*
     *  Data members
     */
    enum {
        kQueueCount = 24   // entries > 2^23 (8Mb) start in last queue
    };

    nsCacheEntryHashTable  mMemCacheEntries;
    bool                   mInitialized;

    PRCList                mEvictionList[kQueueCount];

    int32_t                mHardLimit;
    int32_t                mSoftLimit;

    int32_t                mTotalSize;
    int32_t                mInactiveSize;

    int32_t                mEntryCount;
    int32_t                mMaxEntryCount;
    int32_t                mMaxEntrySize; // internal unit is bytes

    // XXX what other stats do we want to keep?
};


/******************************************************************************
 * nsMemoryCacheDeviceInfo - used to call nsIVisitor for about:cache
 ******************************************************************************/
class nsMemoryCacheDeviceInfo : public nsICacheDeviceInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEDEVICEINFO

    explicit nsMemoryCacheDeviceInfo(nsMemoryCacheDevice* device)
        :   mDevice(device)
    {
    }

private:
    virtual ~nsMemoryCacheDeviceInfo() {}
    nsMemoryCacheDevice* mDevice;
};


#endif // _nsMemoryCacheDevice_h_
