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
 * The Original Code is nsCacheEntry.h, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#ifndef _nsCacheEntry_h_
#define _nsCacheEntry_h_

#include "nspr.h"
#include "pldhash.h"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsAReadableString.h"

#include "nsICacheEntryDescriptor.h"


class nsCacheDevice;
class nsCacheMetaData;
class nsCacheRequest;

class nsCacheEntry
{
public:
    enum CacheEntryFlags {
        eDoomedMask        =   1,
        eEntryDirtyMask    =   2,
        eDataDirtyMask     =   4,
        eMetaDataDirtyMask =   8,
        eStreamDataMask    =  16,
        eActiveMask        =  32,
        eInitializedMask   =  64,
        eValidMask         = 128,
        eAllowInMemoryMask = 256,
        eAllowOnDiskMask   = 512
    };

    nsCacheEntry(nsCString * key);
    ~nsCacheEntry();

    void GetKey( nsCString ** key)            { if (key) *key = mKey; }

    void GetFetchCount( PRUint32 * result)    { if (result) *result = mFetchCount; }
    void SetFetchCount( PRUint32   count)     { mFetchCount = count; }

    void GetLastFetched( PRTime * result)     { if (result) *result = mLastFetched; }
    void SetLastFetched( PRTime   lastFetched)
              { if (lastFetched) mLastFetched = lastFetched; }

    void GetLastValidated( PRTime * result)   { if (result) *result = mLastValidated; }
    void SetLastValidated( PRTime   lastValidated)
              { if (lastValidated) mLastValidated = lastValidated; }

    void GetExpirationTime( PRTime * result)  { if (result) *result = mExpirationTime; }
    void SetExpirationTime( PRTime   expires) { if (expires) mExpirationTime = expires; }

    void GetDataSize( PRUint32 * result)      { if (result) *result = mDataSize; }
    void SetDataSize( PRUint32   size)        { mDataSize = size; }

    void GetMetaDataSize( PRUint32 * result)  { if (result) *result = mMetaSize; }
    void SetMetaDataSize( PRUint32   size)    { mMetaSize = size; }

    void GetCacheDevice( nsCacheDevice ** result) { if (result) *result = mCacheDevice; }
    void SetCacheDevice( nsCacheDevice * device)  { mCacheDevice = device; }

    nsresult GetData( nsISupports ** result);
    nsresult SetData( nsISupports *  data);

    //** should we make these string references instead of pointers?
    nsresult GetMetaDataElement( const nsAReadableCString&  key,
                                 nsAReadableCString **      value);
    nsresult SetMetaDataElement( const nsAReadableCString&  key,
                                 const nsAReadableCString&  value);

    //** enumerate MetaData method

    void MarkEntryDirty()    { mFlags |= eEntryDirtyMask;}
    void MarkDataDirty()     { mFlags |= eDataDirtyMask; }
    void MarkMetaDataDirty() { mFlags |= eMetaDataDirtyMask; }
    void MarkActive()        { mFlags |= eActiveMask; }
    void MarkInactive()      { mFlags &= ~eActiveMask; }
    void MarkValid();
  
    PRBool IsDoomed()        { return (mFlags & eDoomedMask) != 0; }
    PRBool IsEntryDirty()    { return (mFlags & eEntryDirtyMask) != 0; }
    PRBool IsDataDirty()     { return (mFlags & eDataDirtyMask) != 0; }
    PRBool IsMetaDataDirty() { return (mFlags & eMetaDataDirtyMask) != 0; }
    PRBool IsStreamData()    { return (mFlags & eStreamDataMask) != 0; }
    PRBool IsActive()        { return (mFlags & eActiveMask) != 0; }
    PRBool IsInitialized()   { return (mFlags & eInitializedMask) != 0; }
    PRBool IsValid()         { return (mFlags & eValidMask) != 0; }

private:
    friend class nsCacheEntryHashTable;
    friend class nsCacheService;


    // methods for nsCacheService
    nsresult Open(nsCacheRequest *request, nsICacheEntryDescriptor ** result);
    nsresult AsyncOpen(nsCacheRequest *request);
    nsresult Doom() { return NS_ERROR_NOT_IMPLEMENTED; } //** implement

    // internal methods
    nsresult CommonOpen(nsCacheRequest * request, PRUint32 *accessGranted);
    void MarkStreamBased() { mFlags |= eStreamDataMask; }
    void MarkInitialized() { mFlags |= eInitializedMask; }

    nsCString *            mKey;            // 4  //** ask scc about const'ness
    PRUint32               mFetchCount;     // 4
    PRTime                 mLastFetched;    // 8
    PRTime                 mLastValidated;  // 8
    PRTime                 mExpirationTime; // 8
    PRUint32               mFlags;          // 4
    PRUint32               mDataSize;       // 4
    PRUint32               mMetaSize;       // 4
    nsCacheDevice *        mCacheDevice;    // 4
    nsCOMPtr<nsISupports>  mData;           // 4
    nsCacheMetaData *      mMetaData;       // 4
    PRCList                mRequestQ;       // 8 = 64
    PRCList                mDescriptorQ;    // 8 = 72
};




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

    nsCacheEntry *GetEntry( const nsCString * key);
    nsresult      AddEntry( nsCacheEntry *entry);
    nsresult      RemoveEntry( nsCacheEntry *entry);
    //** enumerate entries?

private:

    // PLDHashTable operation callbacks
    static const void *   GetKey( PLDHashTable *table, PLDHashEntryHdr *entry);

    static PLDHashNumber  HashKey( PLDHashTable *table, const void *key);

    static PRBool         MatchEntry( PLDHashTable *           table,
                                      const PLDHashEntryHdr *  entry,
                                      const void *             key);

    static void           MoveEntry( PLDHashTable *table,
                                     const PLDHashEntryHdr *from,
                                     PLDHashEntryHdr       *to);

    static void           ClearEntry( PLDHashTable *table, PLDHashEntryHdr *entry);

    static void           Finalize( PLDHashTable *table);

    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    PRBool                 initialized;
};

#endif // _nsCacheEntry_h_

