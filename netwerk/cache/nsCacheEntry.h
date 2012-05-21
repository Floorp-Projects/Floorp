/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsCacheEntry_h_
#define _nsCacheEntry_h_

#include "nsICache.h"
#include "nsICacheEntryDescriptor.h"
#include "nsIThread.h"
#include "nsCacheMetaData.h"

#include "nspr.h"
#include "pldhash.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAString.h"

class nsCacheDevice;
class nsCacheMetaData;
class nsCacheRequest;
class nsCacheEntryDescriptor;

/******************************************************************************
* nsCacheEntry
*******************************************************************************/
class nsCacheEntry : public PRCList
{
public:

    nsCacheEntry(nsCString *          key,
                 bool                 streamBased,
                 nsCacheStoragePolicy storagePolicy);
    ~nsCacheEntry();


    static nsresult  Create( const char *          key,
                             bool                  streamBased,
                             nsCacheStoragePolicy  storagePolicy,
                             nsCacheDevice *       device,
                             nsCacheEntry **       result);
                                      
    nsCString *  Key()  { return mKey; }

    PRInt32  FetchCount()                              { return mFetchCount; }
    void     SetFetchCount( PRInt32   count)           { mFetchCount = count; }
    void     Fetched();

    PRUint32 LastFetched()                             { return mLastFetched; }
    void     SetLastFetched( PRUint32  lastFetched)    { mLastFetched = lastFetched; }

    PRUint32 LastModified()                            { return mLastModified; }
    void     SetLastModified( PRUint32 lastModified)   { mLastModified = lastModified; }

    PRUint32 ExpirationTime()                     { return mExpirationTime; }
    void     SetExpirationTime( PRUint32 expires) { mExpirationTime = expires; }

    PRUint32 Size()                               
        { return mDataSize + mMetaData.Size() + (mKey ? mKey->Length() : 0); }

    nsCacheDevice * CacheDevice()                            { return mCacheDevice; }
    void            SetCacheDevice( nsCacheDevice * device)  { mCacheDevice = device; }
    const char *    GetDeviceID();

    /**
     * Data accessors
     */
    nsISupports *Data()                           { return mData; }
    void         SetData( nsISupports * data);

    PRInt64  PredictedDataSize()                  { return mPredictedDataSize; }
    void     SetPredictedDataSize(PRInt64 size)   { mPredictedDataSize = size; }

    PRUint32 DataSize()                           { return mDataSize; }
    void     SetDataSize( PRUint32  size)         { mDataSize = size; }

    void     TouchData();
    
    /**
     * Meta data accessors
     */
    const char * GetMetaDataElement( const char *  key) { return mMetaData.GetElement(key); }
    nsresult     SetMetaDataElement( const char *  key,
                                     const char *  value) { return mMetaData.SetElement(key, value); }
    nsresult VisitMetaDataElements( nsICacheMetaDataVisitor * visitor) { return mMetaData.VisitElements(visitor); }
    nsresult FlattenMetaData(char * buffer, PRUint32 bufSize) { return mMetaData.FlattenMetaData(buffer, bufSize); }
    nsresult UnflattenMetaData(const char * buffer, PRUint32 bufSize) { return mMetaData.UnflattenMetaData(buffer, bufSize); }
    PRUint32 MetaDataSize() { return mMetaData.Size(); }  

    void     TouchMetaData();


    /**
     * Security Info accessors
     */
    nsISupports* SecurityInfo() { return mSecurityInfo; }
    void     SetSecurityInfo( nsISupports *  info) { mSecurityInfo = info; }


    // XXX enumerate MetaData method


    enum CacheEntryFlags {
        eStoragePolicyMask   = 0x000000FF,
        eDoomedMask          = 0x00000100,
        eEntryDirtyMask      = 0x00000200,
        eDataDirtyMask       = 0x00000400,
        eMetaDataDirtyMask   = 0x00000800,
        eStreamDataMask      = 0x00001000,
        eActiveMask          = 0x00002000,
        eInitializedMask     = 0x00004000,
        eValidMask           = 0x00008000,
        eBindingMask         = 0x00010000
    };
    
    void MarkBinding()         { mFlags |=  eBindingMask; }
    void ClearBinding()        { mFlags &= ~eBindingMask; }
    bool IsBinding()         { return (mFlags & eBindingMask) != 0; }

    void MarkEntryDirty()      { mFlags |=  eEntryDirtyMask; }
    void MarkEntryClean()      { mFlags &= ~eEntryDirtyMask; }
    void MarkDataDirty()       { mFlags |=  eDataDirtyMask; }
    void MarkDataClean()       { mFlags &= ~eDataDirtyMask; }
    void MarkMetaDataDirty()   { mFlags |=  eMetaDataDirtyMask; }
    void MarkMetaDataClean()   { mFlags &= ~eMetaDataDirtyMask; }
    void MarkStreamData()      { mFlags |=  eStreamDataMask; }
    void MarkValid()           { mFlags |=  eValidMask; }
    void MarkInvalid()         { mFlags &= ~eValidMask; }
    //    void MarkAllowedInMemory() { mFlags |=  eAllowedInMemoryMask; }
    //    void MarkAllowedOnDisk()   { mFlags |=  eAllowedOnDiskMask; }

    bool IsDoomed()          { return (mFlags & eDoomedMask) != 0; }
    bool IsEntryDirty()      { return (mFlags & eEntryDirtyMask) != 0; }
    bool IsDataDirty()       { return (mFlags & eDataDirtyMask) != 0; }
    bool IsMetaDataDirty()   { return (mFlags & eMetaDataDirtyMask) != 0; }
    bool IsStreamData()      { return (mFlags & eStreamDataMask) != 0; }
    bool IsActive()          { return (mFlags & eActiveMask) != 0; }
    bool IsInitialized()     { return (mFlags & eInitializedMask) != 0; }
    bool IsValid()           { return (mFlags & eValidMask) != 0; }
    bool IsInvalid()         { return (mFlags & eValidMask) == 0; }
    bool IsInUse()           { return IsBinding() ||
                                        !(PR_CLIST_IS_EMPTY(&mRequestQ) &&
                                          PR_CLIST_IS_EMPTY(&mDescriptorQ)); }
    bool IsNotInUse()        { return !IsInUse(); }


    bool IsAllowedInMemory()
    {
        return (StoragePolicy() ==  nsICache::STORE_ANYWHERE) ||
            (StoragePolicy() == nsICache::STORE_IN_MEMORY);
    }

    bool IsAllowedOnDisk()
    {
        return (StoragePolicy() == nsICache::STORE_ANYWHERE) ||
            (StoragePolicy() == nsICache::STORE_ON_DISK) ||
            (StoragePolicy() == nsICache::STORE_ON_DISK_AS_FILE);
    }

    bool IsAllowedOffline()
    {
        return (StoragePolicy() == nsICache::STORE_OFFLINE);
    }

    nsCacheStoragePolicy  StoragePolicy()
    {
        return (nsCacheStoragePolicy)(mFlags & eStoragePolicyMask);
    }

    void SetStoragePolicy(nsCacheStoragePolicy policy)
    {
        NS_ASSERTION(policy <= 0xFF, "too many bits in nsCacheStoragePolicy");
        mFlags &= ~eStoragePolicyMask; // clear storage policy bits
        mFlags |= policy;
    }


    // methods for nsCacheService
    nsresult RequestAccess( nsCacheRequest * request, nsCacheAccessMode *accessGranted);
    nsresult CreateDescriptor( nsCacheRequest *           request,
                               nsCacheAccessMode          accessGranted,
                               nsICacheEntryDescriptor ** result);

    //    nsresult Open(nsCacheRequest *request, nsICacheEntryDescriptor ** result);
    //    nsresult AsyncOpen(nsCacheRequest *request);
    bool     RemoveRequest( nsCacheRequest * request);
    bool     RemoveDescriptor( nsCacheEntryDescriptor * descriptor);
    
private:
    friend class nsCacheEntryHashTable;
    friend class nsCacheService;

    void     DetachDescriptors(void);

    // internal methods
    void MarkDoomed()          { mFlags |=  eDoomedMask; }
    void MarkStreamBased()     { mFlags |=  eStreamDataMask; }
    void MarkInitialized()     { mFlags |=  eInitializedMask; }
    void MarkActive()          { mFlags |=  eActiveMask; }
    void MarkInactive()        { mFlags &= ~eActiveMask; }

    nsCString *             mKey;            // 4  // XXX ask scc about const'ness
    PRUint32                mFetchCount;     // 4
    PRUint32                mLastFetched;    // 4
    PRUint32                mLastModified;   // 4
    PRUint32                mLastValidated;  // 4
    PRUint32                mExpirationTime; // 4
    PRUint32                mFlags;          // 4
    PRInt64                 mPredictedDataSize;  // Size given by ContentLength.
    PRUint32                mDataSize;       // 4
    nsCacheDevice *         mCacheDevice;    // 4
    nsCOMPtr<nsISupports>   mSecurityInfo;   // 
    nsISupports *           mData;           // strong ref
    nsCOMPtr<nsIThread>     mThread;
    nsCacheMetaData         mMetaData;       // 4
    PRCList                 mRequestQ;       // 8
    PRCList                 mDescriptorQ;    // 8
};


/******************************************************************************
* nsCacheEntryInfo
*******************************************************************************/
class nsCacheEntryInfo : public nsICacheEntryInfo {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYINFO

    nsCacheEntryInfo(nsCacheEntry* entry)
        :   mCacheEntry(entry)
    {
    }

    virtual ~nsCacheEntryInfo() {}
    void    DetachEntry() { mCacheEntry = nsnull; }
    
private:
    nsCacheEntry * mCacheEntry;
};


/******************************************************************************
* nsCacheEntryHashTable
*******************************************************************************/
typedef struct {
    PLDHashNumber  keyHash;
    nsCacheEntry  *cacheEntry;
} nsCacheEntryHashTableEntry;


class nsCacheEntryHashTable
{
public:
    nsCacheEntryHashTable();
    ~nsCacheEntryHashTable();

    nsresult      Init();
    void          Shutdown();

    nsCacheEntry *GetEntry( const nsCString * key);
    nsresult      AddEntry( nsCacheEntry *entry);
    void          RemoveEntry( nsCacheEntry *entry);
    
    void          VisitEntries( PLDHashEnumerator etor, void *arg);

private:
    // PLDHashTable operation callbacks
    static PLDHashNumber  HashKey( PLDHashTable *table, const void *key);

    static bool           MatchEntry( PLDHashTable *           table,
                                      const PLDHashEntryHdr *  entry,
                                      const void *             key);

    static void           MoveEntry( PLDHashTable *table,
                                     const PLDHashEntryHdr *from,
                                     PLDHashEntryHdr       *to);

    static void           ClearEntry( PLDHashTable *table, PLDHashEntryHdr *entry);

    static void           Finalize( PLDHashTable *table);

    static
    PLDHashOperator       FreeCacheEntries(PLDHashTable *    table,
                                           PLDHashEntryHdr * hdr,
                                           PRUint32          number,
                                           void *            arg);
    static
    PLDHashOperator       VisitEntry(PLDHashTable *         table,
                                     PLDHashEntryHdr *      hdr,
                                     PRUint32               number,
                                     void *                 arg);
                                     
    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    bool                   initialized;
};

#endif // _nsCacheEntry_h_
