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
 * The Original Code is nsMemoryCacheDevice.cpp, released
 * February 22, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gordon Sheridan, <gordon@netscape.com>
 *   Patrick C. Beard <beard@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
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

#include "nsMemoryCacheDevice.h"
#include "nsCacheService.h"
#include "nsICacheService.h"
#include "nsIStorageStream.h"
#include "nsICacheVisitor.h"
#include "nsCRT.h"
#include "nsCache.h"
#include "nsReadableUtils.h"

// The memory cache implements a variation of the "LRU-SP" caching algorithm
// described in "LRU-SP: A Size-Adjusted and Popularity-Aware LRU Replacement
// Algorithm for Web Caching" by Kai Cheng and Yahiko Kambayashi.

// We keep kQueueCount LRU queues, which should be about ceil(log2(mHardLimit))
// The queues hold exponentially increasing ranges of floor(log2((size/nref)))
// values for entries.
// Entries larger than 2^(kQueueCount-1) go in the last queue.
// Entries with no expiration go in the first queue.

const char *gMemoryDeviceID      = "memory";

nsMemoryCacheDevice::nsMemoryCacheDevice()
    : mInitialized(PR_FALSE),
      mEvictionThreshold(PR_INT32_MAX),
      mHardLimit(4 * 1024 * 1024),       // default, if no pref
      mSoftLimit((mHardLimit * 9) / 10), // default, if no pref
      mTotalSize(0),
      mInactiveSize(0),
      mEntryCount(0),
      mMaxEntryCount(0)
{
    for (int i=0; i<kQueueCount; ++i)
        PR_INIT_CLIST(&mEvictionList[i]);
}


nsMemoryCacheDevice::~nsMemoryCacheDevice()
{    
    Shutdown();
}


nsresult
nsMemoryCacheDevice::Init()
{
    if (mInitialized)  return NS_ERROR_ALREADY_INITIALIZED;

    nsresult  rv = mMemCacheEntries.Init();
    mInitialized = NS_SUCCEEDED(rv);
    return rv;
}


nsresult
nsMemoryCacheDevice::Shutdown()
{
    NS_ASSERTION(mInitialized, "### attempting shutdown while not initialized");
    NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);
    
    mMemCacheEntries.Shutdown();

    // evict all entries
    nsCacheEntry * entry, * next;

    for (int i = kQueueCount - 1; i >= 0; --i) {
        entry = (nsCacheEntry *)PR_LIST_HEAD(&mEvictionList[i]);
        while (entry != &mEvictionList[i]) {
            NS_ASSERTION(entry->IsInUse() == PR_FALSE, "### shutting down with active entries.\n");
            next = (nsCacheEntry *)PR_NEXT_LINK(entry);
            PR_REMOVE_AND_INIT_LINK(entry);
        
            // update statistics
            PRInt32 memoryRecovered = (PRInt32)entry->Size();
            mTotalSize    -= memoryRecovered;
            mInactiveSize -= memoryRecovered;
            --mEntryCount;

            delete entry;
            entry = next;
        }
    }

/*
 * we're not factoring in changes to meta data yet...    
 *  NS_ASSERTION(mTotalSize == 0, "### mem cache leaking entries?\n");
 */
    NS_ASSERTION(mInactiveSize == 0, "### mem cache leaking entries?\n");
    NS_ASSERTION(mEntryCount == 0, "### mem cache leaking entries?\n");
    
    mInitialized = PR_FALSE;

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

    // move entry to the tail of an eviction list
    PR_REMOVE_AND_INIT_LINK(entry);
    PR_APPEND_LINK(entry, &mEvictionList[EvictionList(entry, 0)]);
    
    mInactiveSize -= entry->Size();

    return entry;
}


nsresult
nsMemoryCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    if (entry->IsDoomed()) {
#ifdef DEBUG
        // XXX verify we've removed it from mMemCacheEntries & eviction list
#endif
        delete entry;
        return NS_OK;
    }

#ifdef DEBUG
    nsCacheEntry * ourEntry = mMemCacheEntries.GetEntry(entry->Key());
    NS_ASSERTION(ourEntry, "DeactivateEntry called for an entry we don't have!");
    NS_ASSERTION(entry == ourEntry, "entry doesn't match ourEntry");
    if (ourEntry != entry)
        return NS_ERROR_INVALID_POINTER;
#endif

    mInactiveSize += entry->Size();
    EvictEntriesIfNecessary();

    return NS_OK;
}


nsresult
nsMemoryCacheDevice::BindEntry(nsCacheEntry * entry)
{
	if (!entry->IsDoomed()) {
 	    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry),"entry is already on a list!");
	
		// append entry to the eviction list
        PR_APPEND_LINK(entry, &mEvictionList[EvictionList(entry, 0)]);

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
#ifdef DEBUG
    // debug code to verify we have entry
    nsCacheEntry * hashEntry = mMemCacheEntries.GetEntry(entry->Key());
    if (!hashEntry)               NS_WARNING("no entry for key");
    else if (entry != hashEntry)  NS_WARNING("entry != hashEntry");
#endif

    EvictEntry(entry, DO_NOT_DELETE_ENTRY);
}


nsresult
nsMemoryCacheDevice::OpenInputStreamForEntry( nsCacheEntry *    entry,
                                              nsCacheAccessMode mode,
                                              PRUint32          offset,
                                              nsIInputStream ** result)
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    nsCOMPtr<nsIStorageStream> storage;
    nsresult rv;

    nsISupports *data = entry->Data();
    if (data) {
        storage = do_QueryInterface(data, &rv);
        if (NS_FAILED(rv))
            return rv;
    }
    else {
        rv = NS_NewStorageStream(4096, PRUint32(-1), getter_AddRefs(storage));
        if (NS_FAILED(rv))
            return rv;
        entry->SetData(storage);
    }

    return storage->NewInputStream(offset, result);
}


nsresult
nsMemoryCacheDevice::OpenOutputStreamForEntry( nsCacheEntry *     entry,
                                               nsCacheAccessMode  mode,
                                               PRUint32           offset,
                                               nsIOutputStream ** result)
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ENSURE_ARG_POINTER(result);

    nsCOMPtr<nsIStorageStream> storage;
    nsresult rv;

    nsISupports *data = entry->Data();
    if (data) {
        storage = do_QueryInterface(data, &rv);
        if (NS_FAILED(rv))
            return rv;
    }
    else {
        rv = NS_NewStorageStream(4096, PRUint32(-1), getter_AddRefs(storage));
        if (NS_FAILED(rv))
            return rv;
        entry->SetData(storage);
    }

    return storage->GetOutputStream(offset, result);
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
        PRUint32  newSize = entry->DataSize() + deltaSize;
        if ((PRInt32) newSize > mSoftLimit) {
            nsresult rv = nsCacheService::DoomEntry(entry);
            NS_ASSERTION(NS_SUCCEEDED(rv),"DoomEntry() failed.");
            return NS_ERROR_ABORT;
        }
    }

    // adjust our totals
    mTotalSize    += deltaSize;
    
    if (!entry->IsDoomed()) {
        // move entry to the tail of the appropriate eviction list
        PR_REMOVE_AND_INIT_LINK(entry);
        PR_APPEND_LINK(entry, &mEvictionList[EvictionList(entry, deltaSize)]);
    }

    EvictEntriesIfNecessary();
    return NS_OK;
}


void
nsMemoryCacheDevice::AdjustMemoryLimits(PRInt32  softLimit, PRInt32  hardLimit)
{
    mSoftLimit = softLimit;
    mHardLimit = hardLimit;

    // First, evict entries that won't fit into the new cache size.
    EvictEntriesIfNecessary();
}


void
nsMemoryCacheDevice::EvictEntry(nsCacheEntry * entry, PRBool deleteEntry)
{
    // remove entry from our hashtable
    mMemCacheEntries.RemoveEntry(entry);
    
    // remove entry from the eviction list
    PR_REMOVE_AND_INIT_LINK(entry);
    
    // update statistics
    PRInt32 memoryRecovered = (PRInt32)entry->Size();
    mTotalSize    -= memoryRecovered;
    if (!entry->IsDoomed())
        mInactiveSize -= memoryRecovered;
    --mEntryCount;
    
    if (deleteEntry)  delete entry;
}


void
nsMemoryCacheDevice::EvictEntriesIfNecessary(void)
{
    nsCacheEntry * entry, * next;

    if ((mTotalSize < mHardLimit) && (mInactiveSize < mSoftLimit))
        return;

    for (int i = kQueueCount - 1; i >= 0; --i) {
        entry = (nsCacheEntry *)PR_LIST_HEAD(&mEvictionList[i]);
        while (entry != &mEvictionList[i]) {
            if (entry->IsInUse()) {
                entry = (nsCacheEntry *)PR_NEXT_LINK(entry);
                continue;
            }

            next = (nsCacheEntry *)PR_NEXT_LINK(entry);
            EvictEntry(entry, DELETE_ENTRY);
            entry = next;

            if ((mTotalSize < mHardLimit) && (mInactiveSize < mSoftLimit))
                return;
        }
    }
}


int
nsMemoryCacheDevice::EvictionList(nsCacheEntry * entry, PRInt32  deltaSize)
{
    // favor items which never expire by putting them in the lowest-index queue
    if (entry->ExpirationTime() == NO_EXPIRATION_TIME)
        return 0;

    // compute which eviction queue this entry should go into,
    // based on floor(log2(size/nref))
    PRInt32  size       = deltaSize + (PRInt32)entry->Size();
    PRInt32  fetchCount = PR_MAX(1, entry->FetchCount());

    return PR_MIN(PR_FloorLog2(size / fetchCount), kQueueCount - 1);
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

    for (int i = kQueueCount - 1; i >= 0; --i) {
        entry = (nsCacheEntry *)PR_LIST_HEAD(&mEvictionList[i]);
        while (entry != &mEvictionList[i]) {
            nsCacheEntryInfo * entryInfo = new nsCacheEntryInfo(entry);
            if (!entryInfo) return NS_ERROR_OUT_OF_MEMORY;
            entryRef = entryInfo;

            rv = visitor->VisitEntry(gMemoryDeviceID, entryInfo, &keepGoing);
            entryInfo->DetachEntry();
            if (NS_FAILED(rv)) return rv;
            if (!keepGoing) break;

            entry = (nsCacheEntry *)PR_NEXT_LINK(entry);
        }
    }
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::EvictEntries(const char * clientID)
{
    nsCacheEntry * entry;
    PRUint32 prefixLength = (clientID ? strlen(clientID) : 0);

    for (int i = kQueueCount - 1; i >= 0; --i) {
        PRCList * elem = PR_LIST_HEAD(&mEvictionList[i]);
        while (elem != &mEvictionList[i]) {
            entry = (nsCacheEntry *)elem;
            elem = PR_NEXT_LINK(elem);
            
            const char * key = entry->Key()->get();
            if (clientID && nsCRT::strncmp(clientID, key, prefixLength) != 0)
                continue;
            
            if (entry->IsInUse()) {
                nsresult rv = nsCacheService::DoomEntry(entry);
                if (NS_FAILED(rv)) {
                    CACHE_LOG_WARNING(("memCache->EvictEntries() aborted: rv =%x", rv));
                    return rv;
                }
            } else {
                EvictEntry(entry, DELETE_ENTRY);
            }
        }
    }

    return NS_OK;
}


// WARNING: SetCapacity can get called before Init()
void
nsMemoryCacheDevice::SetCapacity(PRInt32  capacity)
{
    PRInt32 hardLimit = capacity * 1024;  // convert k into bytes
    PRInt32 softLimit = (hardLimit * 9) / 10;
    AdjustMemoryLimits(softLimit, hardLimit);
}


#ifdef DEBUG
class nsCacheHashCounter : public nsCacheEntryHashTable::Visitor {
public:
    nsCacheHashCounter() : entryCount(0) {}

    PRBool  VisitEntry(nsCacheEntry * entry);
    PRInt32 entryCount;
};


PRBool nsCacheHashCounter::VisitEntry(nsCacheEntry * entry)
{
    ++entryCount;
    return PR_TRUE;
}


void
nsMemoryCacheDevice::CheckEntryCount()
{
    if (!mInitialized)  return;

    PRInt32 evictionListCount = 0;
    for (int i=0; i<kQueueCount; ++i) {
        PRCList * elem = PR_LIST_HEAD(&mEvictionList[i]);
        while (elem != &mEvictionList[i]) {
            elem = PR_NEXT_LINK(elem);
            ++evictionListCount;
        }
    }
    NS_ASSERTION(mEntryCount == evictionListCount, "### mem cache badness");

    nsCacheHashCounter hash;
    mMemCacheEntries.VisitEntries(&hash);
    NS_ASSERTION(mEntryCount == hash.entryCount, "### mem cache badness");    
}
#endif

/******************************************************************************
 * nsMemoryCacheDeviceInfo - for implementing about:cache
 *****************************************************************************/


NS_IMPL_ISUPPORTS1(nsMemoryCacheDeviceInfo, nsICacheDeviceInfo)


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
    nsCString  buffer;

    buffer.Append("\n<tr>\n<td><b>Inactive storage:</b></td>\n<td><tt> ");
    buffer.AppendInt(mDevice->mInactiveSize / 1024);
    buffer.Append(" KiB</tt></td>\n</tr>\n");
    
    *result = ToNewCString(buffer);
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetEntryCount(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    // XXX compare calculated count vs. mEntryCount
    *result = (PRUint32)mDevice->mEntryCount;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetTotalSize(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = (PRUint32)mDevice->mTotalSize;
    return NS_OK;
}


NS_IMETHODIMP
nsMemoryCacheDeviceInfo::GetMaximumSize(PRUint32 * result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = (PRUint32)mDevice->mHardLimit;
    return NS_OK;
}
