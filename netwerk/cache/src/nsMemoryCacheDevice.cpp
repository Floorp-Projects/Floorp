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
 * The Original Code is nsMemoryCacheDevice.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#include "nsMemoryCacheDevice.h"
#include "nsICacheService.h"
#include "nsIComponentManager.h"
#include "nsNetCID.h"
#include "nsIObserverService.h"
#include "nsIPref.h"


static NS_DEFINE_CID(kStorageTransportCID, NS_STORAGETRANSPORT_CID);

const char *gMemoryCacheSizePref = "browser.cache.memory_cache_size";

nsMemoryCacheDevice::nsMemoryCacheDevice()
    : mHardLimit(0),
      mSoftLimit(0),
      mTotalSize(0),
      mInactiveSize(0),
      mEntryCount(0),
      mMaxEntryCount(0)
{
    PR_INIT_CLIST(&mEvictionList);
}

nsMemoryCacheDevice::~nsMemoryCacheDevice()
{
    // XXX dealloc all memory
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        prefs->UnregisterCallback(gMemoryCacheSizePref, MemoryCacheSizeChanged, this);
    }
}


int PR_CALLBACK
nsMemoryCacheDevice::MemoryCacheSizeChanged(const char * pref, void * closure)
{
    nsresult  rv;
    PRUint32   softLimit = 0;
    nsMemoryCacheDevice * device = (nsMemoryCacheDevice *)closure;

    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv))  return rv;

    rv = prefs->GetIntPref(gMemoryCacheSizePref, (PRInt32 *)&softLimit);
    
    PRUint32 hardLimit = softLimit + 1024*1024*2;  // XXX find better limit than +2Meg
    rv = device->AdjustMemoryLimits(softLimit, hardLimit);

    return 0; // XXX what are we supposed to return?
}


nsresult
nsMemoryCacheDevice::Init()
{
    nsresult  rv;

    rv = mMemCacheEntries.Init();

    // XXX read user prefs for memory cache limits
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_FAILED(rv))  return rv;

    rv = prefs->RegisterCallback(gMemoryCacheSizePref, MemoryCacheSizeChanged, this);
    if (NS_FAILED(rv))  return rv;

    // Initialize the pref
    MemoryCacheSizeChanged(gMemoryCacheSizePref, this);

    // Register as a memory pressure observer
    NS_WITH_SERVICE(nsIObserverService,
                    observerService,
                    NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        // XXX rv = observerServcie->AddObserver(this, NS_MEMORY_PRESSURE_TOPIC);
    }
    // Ignore failure of memory pressure registration

    return rv;
}


const char *
nsMemoryCacheDevice::GetDeviceID()
{
    return "memory";
}


nsCacheEntry *
nsMemoryCacheDevice::FindEntry(nsCString * key)
{
    nsCacheEntry * entry = mMemCacheEntries.GetEntry(key);
    if (!entry)  return nsnull;

    // move entry to the tail of the eviction list
    PR_REMOVE_AND_INIT_LINK(entry);
    PR_APPEND_LINK(entry, &mEvictionList);
    
    return entry;;
}


nsresult
nsMemoryCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    if (entry->IsDoomed()) {
#if debug
        // XXX verify we've removed it from mMemCacheEntries & eviction list
#endif
        delete entry;
        return NS_OK;
    }

    nsCacheEntry * ourEntry = mMemCacheEntries.GetEntry(entry->Key());
    NS_ASSERTION(ourEntry, "DeactivateEntry called for an entry we don't have!");
    NS_ASSERTION(entry == ourEntry, "entry doesn't match ourEntry");
    if (ourEntry != entry)
        return NS_ERROR_INVALID_POINTER;

    return NS_OK;
}


nsresult
nsMemoryCacheDevice::BindEntry(nsCacheEntry * entry)
{
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry),"entry is already on a list!");

    // append entry to the eviction list
    PR_APPEND_LINK(entry, &mEvictionList);

    // add entry to hashtable of mem cache entries
    nsresult  rv = mMemCacheEntries.AddEntry(entry);
    if (NS_FAILED(rv)) {
        PR_REMOVE_AND_INIT_LINK(entry);
        return rv;
    }

    // XXX add size of entry to memory totals
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    nsresult  rv = mMemCacheEntries.RemoveEntry(entry);
    NS_ASSERTION(NS_SUCCEEDED(rv), "dooming entry that we don't have");
    if (NS_FAILED(rv)) return rv;

    // remove entry from our eviction list
    PR_REMOVE_AND_INIT_LINK(entry);

    return NS_OK;
}


nsresult
nsMemoryCacheDevice::GetTransportForEntry( nsCacheEntry *    entry,
                                           nsCacheAccessMode mode,
                                           nsITransport   ** transport )
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(transport);

    nsCOMPtr<nsISupports> data;

    nsresult rv = entry->GetData(getter_AddRefs(data));
    if (NS_FAILED(rv))
        return rv;

    if (data)
        return CallQueryInterface(data, transport);
    else {
        // create a new transport for this entry
        rv = nsComponentManager::CreateInstance(kStorageTransportCID,
                                                nsnull,
                                                NS_GET_IID(nsITransport),
                                                (void **) transport);
        if (NS_FAILED(rv)) return rv;

        return entry->SetData(*transport);
    }
}


nsresult
nsMemoryCacheDevice::OnDataSizeChange( nsCacheEntry * entry, PRInt32 deltaSize)
{
    // XXX keep track of totals
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::AdjustMemoryLimits(PRUint32  softLimit, PRUint32  hardLimit)
{
    mSoftLimit = softLimit;
    mHardLimit = hardLimit;

    if ((mTotalSize > mHardLimit) || (mInactiveSize > softLimit)) {
        // XXX rv = EvictEntries();
    }
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::EvictEntries(void)
{
    nsCacheEntry * entry;
    nsresult       rv = NS_OK;

    entry = (nsCacheEntry *)PR_LIST_HEAD(&mEvictionList);
    while (entry != &mEvictionList) {
        if (entry->IsInUse()) {
            entry = (nsCacheEntry *)PR_NEXT_LINK(entry);
            continue;
        }

        // remove entry from our hashtable
        rv = mMemCacheEntries.RemoveEntry(entry);
        NS_ASSERTION(NS_SUCCEEDED(rv), "RemoveEntry() failed?");

        // remove entry from the eviction list
        PR_REMOVE_AND_INIT_LINK(entry);
        
        // update statistics
        PRUint32 memoryRecovered = entry->DataSize() + entry->MetaDataSize();
        mTotalSize    -= memoryRecovered;
        mInactiveSize -= memoryRecovered;
        --mEntryCount;
        delete entry;

        if ((mTotalSize < mHardLimit) && (mInactiveSize < mSoftLimit))
            break;
    }

    return NS_OK;
}



// XXX need methods for enumerating entries


