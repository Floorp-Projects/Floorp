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
 *    Gordon Sheridan, <gordon@netscape.com>
 *    Patrick C. Beard <beard@netscape.com>
 */

#include "nsMemoryCacheDevice.h"
#include "nsCacheService.h"
#include "nsICacheService.h"
#include "nsIComponentManager.h"
#include "nsNetCID.h"
#include "nsIObserverService.h"
#include "nsIPref.h"
#include "nsICacheVisitor.h"
#include "nsITransport.h"
#include "signal.h"


static NS_DEFINE_CID(kStorageTransportCID, NS_STORAGETRANSPORT_CID);

const char *gMemoryDeviceID      = "memory";
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
#if DEBUG
    printf("### starting ~nsMemoryCacheDevice()\n");
#endif
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
    if (NS_FAILED(rv)) return rv;
    
    softLimit *= 1024;  // convert k into bytes
    PRUint32 hardLimit = softLimit + 1024*1024*2;  // XXX find better limit than +2Meg
    device->AdjustMemoryLimits(softLimit, hardLimit);

    return 0; // XXX what are we supposed to return?
}


nsresult
nsMemoryCacheDevice::Init()
{
    nsresult  rv;

    rv = mMemCacheEntries.Init();
    
    // set some default memory limits, in case prefs aren't available
    mSoftLimit = 1024 * 1024 * 3;
    mHardLimit = mSoftLimit + 1024 *1024 * 2;

    // read user prefs for memory cache limits
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {

        rv = prefs->RegisterCallback(gMemoryCacheSizePref, MemoryCacheSizeChanged, this);
        if (NS_FAILED(rv))  return rv;

        // Initialize the pref
        MemoryCacheSizeChanged(gMemoryCacheSizePref, this);
    }

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

nsresult
nsMemoryCacheDevice::Shutdown()
{
    return NS_OK;
}

const char *
nsMemoryCacheDevice::GetDeviceID()
{
    return gMemoryDeviceID;
}


nsCacheEntry *
nsMemoryCacheDevice::FindEntry(nsCString * key)
{
    nsCacheEntry * entry = mMemCacheEntries.GetEntry(key);
    if (!entry)  return nsnull;

    // move entry to the tail of the eviction list
    PR_REMOVE_AND_INIT_LINK(entry);
    PR_APPEND_LINK(entry, &mEvictionList);
    
    mInactiveSize -= entry->Size();

    return entry;
}


nsresult
nsMemoryCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    if (entry->IsDoomed()) {
#if debug
        // XXX verify we've removed it from mMemCacheEntries & eviction list
#endif
        // update statistics
        mTotalSize    -= entry->Size();
        --mEntryCount;

        delete entry;
        return NS_OK;
    }

    nsCacheEntry * ourEntry = mMemCacheEntries.GetEntry(entry->Key());
    NS_ASSERTION(ourEntry, "DeactivateEntry called for an entry we don't have!");
    NS_ASSERTION(entry == ourEntry, "entry doesn't match ourEntry");
    if (ourEntry != entry)
        return NS_ERROR_INVALID_POINTER;

    mInactiveSize += entry->Size();
    EvictEntriesIfNecessary();

    return NS_OK;
}


nsresult
nsMemoryCacheDevice::BindEntry(nsCacheEntry * entry)
{
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry),"entry is already on a list!");

	if (!entry->IsDoomed()) {
        // append entry to the eviction list
	    PR_APPEND_LINK(entry, &mEvictionList);

        // add entry to hashtable of mem cache entries
        nsresult  rv = mMemCacheEntries.AddEntry(entry);
        if (NS_FAILED(rv)) {
            PR_REMOVE_AND_INIT_LINK(entry);
            return rv;
        }
	}

    // add size of entry to memory totals
    ++mEntryCount;
    if (mMaxEntryCount < mEntryCount) mMaxEntryCount = mEntryCount;

    mTotalSize += entry->Size();
    EvictEntriesIfNecessary();
    
    return NS_OK;
}


void
nsMemoryCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    // XXX debug code to verify we have entry
    mMemCacheEntries.RemoveEntry(entry);

    // remove entry from our eviction list
    PR_REMOVE_AND_INIT_LINK(entry);
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

        entry->SetData(*transport);
        return NS_OK;
    }
}


nsresult
nsMemoryCacheDevice::GetFileForEntry( nsCacheEntry *    entry,
                                      nsIFile **        result )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsMemoryCacheDevice::OnDataSizeChange( nsCacheEntry * entry, PRInt32 deltaSize)
{
    if (entry->IsStreamData()) {
        // we have the right to refuse or pre-evict
    }

    // adjust our totals
    mTotalSize    += deltaSize;

    EvictEntriesIfNecessary();

    return NS_OK;
}


void
nsMemoryCacheDevice::AdjustMemoryLimits(PRUint32  softLimit, PRUint32  hardLimit)
{
    mSoftLimit = softLimit;
    mHardLimit = hardLimit;
    EvictEntriesIfNecessary();
}


void
nsMemoryCacheDevice::EvictEntry(nsCacheEntry * entry)
{
    // remove entry from our hashtable
    mMemCacheEntries.RemoveEntry(entry);
    
    // remove entry from the eviction list
    PR_REMOVE_AND_INIT_LINK(entry);
    
    // update statistics
    PRUint32 memoryRecovered = entry->Size();
    mTotalSize    -= memoryRecovered;
    mInactiveSize -= memoryRecovered;
    --mEntryCount;

    delete entry;
}


void
nsMemoryCacheDevice::EvictEntriesIfNecessary(void)
{
    nsCacheEntry * entry, * next;

    if ((mTotalSize < mHardLimit) && (mInactiveSize < mSoftLimit))
        return;

    // XXX implement more sophisticated eviction ordering

    entry = (nsCacheEntry *)PR_LIST_HEAD(&mEvictionList);
    while (entry != &mEvictionList) {
        if (entry->IsInUse()) {
            entry = (nsCacheEntry *)PR_NEXT_LINK(entry);
            continue;
        }

        next = (nsCacheEntry *)PR_NEXT_LINK(entry);
        EvictEntry(entry);
        entry = next;

        if ((mTotalSize < mHardLimit) && (mInactiveSize < mSoftLimit))
            break;
    }
}


nsresult
nsMemoryCacheDevice::Visit(nsICacheVisitor * visitor)
{
    nsMemoryCacheDeviceInfo * deviceInfo = new nsMemoryCacheDeviceInfo(this);
    nsCOMPtr<nsICacheDeviceInfo> deviceRef(deviceInfo);
    if (!deviceInfo) return NS_ERROR_OUT_OF_MEMORY;

    PRBool keepGoing;
    nsresult rv = visitor->VisitDevice(gMemoryDeviceID, deviceInfo, &keepGoing);
    if (NS_FAILED(rv)) return rv;

    if (!keepGoing)
        return NS_OK;

    nsCacheEntry *              entry;
    nsCOMPtr<nsICacheEntryInfo> entryRef;

    entry = (nsCacheEntry *)PR_LIST_HEAD(&mEvictionList);
    while (entry != &mEvictionList) {
        nsCacheEntryInfo * entryInfo = new nsCacheEntryInfo(entry);
        if (!entryInfo) return NS_ERROR_OUT_OF_MEMORY;
        entryRef = entryInfo;

        rv = visitor->VisitEntry(gMemoryDeviceID, entryInfo, &keepGoing);
        entryInfo->DetachEntry();
        if (NS_FAILED(rv)) return rv;
        if (!keepGoing) break;

        entry = (nsCacheEntry *)PR_NEXT_LINK(entry);
    }

    return NS_OK;
}


nsresult
nsMemoryCacheDevice::EvictEntries(const char * clientID)
{
    nsCacheEntry * entry;
    PRUint32 prefixLength = (clientID ? nsCRT::strlen(clientID) : 0);

    PRCList * elem = PR_LIST_HEAD(&mEvictionList);
    while (elem != &mEvictionList) {
        entry = (nsCacheEntry *)elem;
        elem = PR_NEXT_LINK(elem);
        
        const char * key = entry->Key()->get();
        if (clientID && nsCRT::strncmp(clientID, key, prefixLength) != 0)
            continue;
        
        if (entry->IsInUse()) {
            nsresult rv = nsCacheService::GlobalInstance()->DoomEntry_Locked(entry);
            if (NS_FAILED(rv)) return rv;
        } else {
            EvictEntry(entry);
        }
    }
    return NS_OK;
}


/******************************************************************************
 * nsMemoryCacheDeviceInfo - for implementing about:cache
 *****************************************************************************/


NS_IMPL_ISUPPORTS1(nsMemoryCacheDeviceInfo, nsICacheDeviceInfo);


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetDescription(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = nsCRT::strdup("Memory cache device");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetUsageReport(char ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = nsCRT::strdup("Memory cache usage report:");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetEntryCount(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    // XXX compare calculated count vs. mEntryCount
    *result = mDevice->mEntryCount;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetTotalSize(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = mDevice->mTotalSize;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetMaximumSize(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = mDevice->mHardLimit;
    return NS_OK;
}
