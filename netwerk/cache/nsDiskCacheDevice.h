/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsDiskCacheDevice_h_
#define _nsDiskCacheDevice_h_

#include "nsCacheDevice.h"
#include "nsDiskCacheBinding.h"
#include "nsDiskCacheBlockFile.h"
#include "nsDiskCacheEntry.h"

#include "nsILocalFile.h"
#include "nsIObserver.h"
#include "nsCOMArray.h"

class nsDiskCacheMap;


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
                                             PRUint32          offset,
                                             nsIInputStream ** result);

    virtual nsresult OpenOutputStreamForEntry(nsCacheEntry *     entry,
                                              nsCacheAccessMode  mode,
                                              PRUint32           offset,
                                              nsIOutputStream ** result);

    virtual nsresult        GetFileForEntry(nsCacheEntry *    entry,
                                            nsIFile **        result);

    virtual nsresult        OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize);
    
    virtual nsresult        Visit(nsICacheVisitor * visitor);

    virtual nsresult        EvictEntries(const char * clientID);

    bool                    EntryIsTooBig(PRInt64 entrySize);

    /**
     * Preference accessors
     */
    void                    SetCacheParentDirectory(nsILocalFile * parentDir);
    void                    SetCapacity(PRUint32  capacity);
    void                    SetMaxEntrySize(PRInt32  maxSizeInKilobytes);

/* private: */

    void                    getCacheDirectory(nsILocalFile ** result);
    PRUint32                getCacheCapacity();
    PRUint32                getCacheSize();
    PRUint32                getEntryCount();
    
    nsDiskCacheMap *        CacheMap()    { return &mCacheMap; }
    
private:    
    friend class nsDiskCacheDeviceDeactivateEntryEvent;
    friend class nsEvictDiskCacheEntriesEvent;
    /**
     *  Private methods
     */

    inline bool IsValidBinding(nsDiskCacheBinding *binding)
    {
        NS_ASSERTION(binding, "  binding == nsnull");
        NS_ASSERTION(binding->mDeactivateEvent == nsnull,
                     "  entry in process of deactivation");
        return (binding && !binding->mDeactivateEvent);
    }

    bool                    Initialized() { return mInitialized; }

    nsresult                Shutdown_Private(bool flush);
    nsresult                DeactivateEntry_Private(nsCacheEntry * entry,
                                                    nsDiskCacheBinding * binding);

    nsresult                OpenDiskCache();
    nsresult                ClearDiskCache();

    nsresult                EvictDiskCacheEntries(PRUint32  targetCapacity);
    
    /**
     *  Member variables
     */
    nsCOMPtr<nsILocalFile>  mCacheDirectory;
    nsDiskCacheBindery      mBindery;
    PRUint32                mCacheCapacity;     // Unit is KiB's
    PRInt32                 mMaxEntrySize;      // Unit is bytes internally
    // XXX need soft/hard limits, currentTotal
    nsDiskCacheMap          mCacheMap;
    bool                    mInitialized;
    bool                    mClearingDiskCache;
};

#endif // _nsDiskCacheDevice_h_
