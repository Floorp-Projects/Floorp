/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsMemoryCacheDevice_h_
#define _nsMemoryCacheDevice_h_

#include "nsCacheDevice.h"
#include "pldhash.h"
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

    virtual nsresult        Init();
    virtual nsresult        Shutdown();

    virtual const char *    GetDeviceID(void);

    virtual nsresult        BindEntry( nsCacheEntry * entry );
    virtual nsCacheEntry *  FindEntry( nsCString * key, bool *collision );
    virtual void            DoomEntry( nsCacheEntry * entry );
    virtual nsresult        DeactivateEntry( nsCacheEntry * entry );

    virtual nsresult OpenInputStreamForEntry(nsCacheEntry *     entry,
                                             nsCacheAccessMode  mode,
                                             PRUint32           offset,
                                             nsIInputStream **  result);

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIOutputStream ** result);

    virtual nsresult GetFileForEntry( nsCacheEntry *    entry,
                                      nsIFile **        result );

    virtual nsresult OnDataSizeChange( nsCacheEntry * entry, PRInt32 deltaSize );

    virtual nsresult Visit( nsICacheVisitor * visitor );

    virtual nsresult EvictEntries(const char * clientID);
    
    void             SetCapacity(PRInt32  capacity);
    void             SetMaxEntrySize(PRInt32  maxSizeInKilobytes);

    bool             EntryIsTooBig(PRInt64 entrySize);

    size_t           TotalSize();

private:
    friend class nsMemoryCacheDeviceInfo;
    enum      { DELETE_ENTRY        = true,
                DO_NOT_DELETE_ENTRY = false };

    void      AdjustMemoryLimits( PRInt32  softLimit, PRInt32  hardLimit);
    void      EvictEntry( nsCacheEntry * entry , bool deleteEntry);
    void      EvictEntriesIfNecessary();
    int       EvictionList(nsCacheEntry * entry, PRInt32  deltaSize);

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
    PRInt32                mEvictionThreshold;

    PRInt32                mHardLimit;
    PRInt32                mSoftLimit;

    PRInt32                mTotalSize;
    PRInt32                mInactiveSize;

    PRInt32                mEntryCount;
    PRInt32                mMaxEntryCount;
    PRInt32                mMaxEntrySize; // internal unit is bytes

    // XXX what other stats do we want to keep?
};


/******************************************************************************
 * nsMemoryCacheDeviceInfo - used to call nsIVisitor for about:cache
 ******************************************************************************/
class nsMemoryCacheDeviceInfo : public nsICacheDeviceInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEDEVICEINFO

    nsMemoryCacheDeviceInfo(nsMemoryCacheDevice* device)
        :   mDevice(device)
    {
    }

    virtual ~nsMemoryCacheDeviceInfo() {}
    
private:
    nsMemoryCacheDevice* mDevice;
};


#endif // _nsMemoryCacheDevice_h_
