/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsCacheEntry.h, released
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan <gordon@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#define NO_EXPIRATION_TIME  0xFFFFFFFF

/******************************************************************************
* nsCacheEntry
*******************************************************************************/
class nsCacheEntry : public PRCList
{
public:

    nsCacheEntry(nsCString *          key,
                 PRBool               streamBased,
                 nsCacheStoragePolicy storagePolicy);
    ~nsCacheEntry();


    static nsresult  Create( const char *          key,
                             PRBool                streamBased,
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

    PRUint32 Size()                               { return mDataSize + mMetaData.Size(); }

    nsCacheDevice * CacheDevice()                            { return mCacheDevice; }
    void            SetCacheDevice( nsCacheDevice * device)  { mCacheDevice = device; }
    const char *    GetDeviceID();

    /**
     * Data accessors
     */
    nsISupports *Data()                           { return mData; }
    void         SetData( nsISupports * data);

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
    nsresult GetSecurityInfo( nsISupports ** result);
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
    PRBool IsBinding()         { return (mFlags & eBindingMask) != 0; }

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

    PRBool IsDoomed()          { return (mFlags & eDoomedMask) != 0; }
    PRBool IsEntryDirty()      { return (mFlags & eEntryDirtyMask) != 0; }
    PRBool IsDataDirty()       { return (mFlags & eDataDirtyMask) != 0; }
    PRBool IsMetaDataDirty()   { return (mFlags & eMetaDataDirtyMask) != 0; }
    PRBool IsStreamData()      { return (mFlags & eStreamDataMask) != 0; }
    PRBool IsActive()          { return (mFlags & eActiveMask) != 0; }
    PRBool IsInitialized()     { return (mFlags & eInitializedMask) != 0; }
    PRBool IsValid()           { return (mFlags & eValidMask) != 0; }
    PRBool IsInvalid()         { return (mFlags & eValidMask) == 0; }
    PRBool IsInUse()           { return IsBinding() ||
                                        !(PR_CLIST_IS_EMPTY(&mRequestQ) &&
                                          PR_CLIST_IS_EMPTY(&mDescriptorQ)); }
    PRBool IsNotInUse()        { return !IsInUse(); }


    PRBool IsAllowedInMemory()
    {
        return (StoragePolicy() ==  nsICache::STORE_ANYWHERE) ||
            (StoragePolicy() == nsICache::STORE_IN_MEMORY);
    }

    PRBool IsAllowedOnDisk()
    {
        return (StoragePolicy() == nsICache::STORE_ANYWHERE) ||
            (StoragePolicy() == nsICache::STORE_ON_DISK) ||
            (StoragePolicy() == nsICache::STORE_ON_DISK_AS_FILE);
    }

    PRBool IsAllowedOffline()
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
    PRBool   RemoveRequest( nsCacheRequest * request);
    PRBool   RemoveDescriptor( nsCacheEntryDescriptor * descriptor);
    
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
    
    // XXX enumerate entries?
    class Visitor {
    public:
        virtual PRBool VisitEntry( nsCacheEntry *entry) = 0;
    };
    
    void          VisitEntries( Visitor *visitor);
    
private:
    friend class nsCacheService; // XXX redefine interface so this isn't necessary

    // PLDHashTable operation callbacks
    static PLDHashNumber  PR_CALLBACK HashKey( PLDHashTable *table, const void *key);

    static PRBool         PR_CALLBACK MatchEntry( PLDHashTable *           table,
                                                  const PLDHashEntryHdr *  entry,
                                                  const void *             key);

    static void           PR_CALLBACK MoveEntry( PLDHashTable *table,
                                                 const PLDHashEntryHdr *from,
                                                 PLDHashEntryHdr       *to);

    static void           PR_CALLBACK ClearEntry( PLDHashTable *table, PLDHashEntryHdr *entry);

    static void           PR_CALLBACK Finalize( PLDHashTable *table);

    static
    PLDHashOperator       PR_CALLBACK FreeCacheEntries(PLDHashTable *    table,
                                                       PLDHashEntryHdr * hdr,
                                                       PRUint32          number,
                                                       void *            arg);
    static
    PLDHashOperator       PR_CALLBACK VisitEntry(PLDHashTable *         table,
                                                 PLDHashEntryHdr *      hdr,
                                                 PRUint32               number,
                                                 void *                 arg);
                                     
    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    PRBool                 initialized;
};

#endif // _nsCacheEntry_h_

