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
 * The Original Code is nsMemoryCacheDevice.h, released February 20, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 20-February-2001
 */

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
    virtual nsCacheEntry *  FindEntry( nsCString * key );
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
 
private:
    friend class nsMemoryCacheDeviceInfo;
    void      AdjustMemoryLimits( PRInt32  softLimit, PRInt32  hardLimit);
    void      EvictEntry( nsCacheEntry * entry );
    void      EvictEntriesIfNecessary();
    int       EvictionList(nsCacheEntry * entry, PRInt32  deltaSize);

    /*
     *  Data members
     */
    
    nsCacheEntryHashTable   mMemCacheEntries;
    PRBool                  mInitialized;
    
    enum { mostLikelyToEvict = 0, leastLikelyToEvict = 1 };   // constants to differentiate eviction lists
    PRCList                 mEvictionList[2];
    PRInt32                mEvictionThreshold;

    PRInt32                mHardLimit;
    PRInt32                mSoftLimit;

    PRInt32                mTotalSize;
    PRInt32                mInactiveSize;

    PRInt32                mEntryCount;

    PRInt32                mMaxEntryCount;
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
