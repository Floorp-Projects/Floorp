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

    virtual nsresult GetTransportForEntry( nsCacheEntry * entry,
                                           nsCacheAccessMode mode,
                                           nsITransport **transport );

    virtual nsresult GetFileForEntry( nsCacheEntry *    entry,
                                      nsIFile **        result );

    virtual nsresult OnDataSizeChange( nsCacheEntry * entry, PRInt32 deltaSize );

    virtual nsresult Visit( nsICacheVisitor * visitor );

    virtual nsresult        EvictEntries(const char * clientID);
    
    static int PR_CALLBACK MemoryCacheSizeChanged(const char * pref, void * closure);
 
private:
    friend class nsMemoryCacheDeviceInfo;
    void      AdjustMemoryLimits(PRUint32  softLimit, PRUint32  hardLimit);
    void      EvictEntriesIfNecessary(void);

    nsCacheEntryHashTable   mMemCacheEntries;
    PRCList                 mEvictionList;

    PRUint32                mHardLimit;
    PRUint32                mSoftLimit;

    PRUint32                mTotalSize;
    PRUint32                mInactiveSize;

    PRUint32                mEntryCount;

    PRUint32                mMaxEntryCount;
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
        NS_INIT_ISUPPORTS();
    }

    virtual ~nsMemoryCacheDeviceInfo() {}
    
private:
    nsMemoryCacheDevice* mDevice;
};


#endif // _nsMemoryCacheDevice_h_
