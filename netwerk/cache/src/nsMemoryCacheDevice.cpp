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

static NS_DEFINE_CID(kStorageTransportCID, NS_STORAGETRANSPORT_CID);

nsMemoryCacheDevice::nsMemoryCacheDevice()
    : mCurrentTotal(0)
{
    PR_INIT_CLIST(&mEvictionList);
}

nsMemoryCacheDevice::~nsMemoryCacheDevice()
{
    //** dealloc all memory
}


nsresult
nsMemoryCacheDevice::Init()
{
    nsresult  rv;

    rv = mMemCacheEntries.Init();

    //** read user prefs for memory cache limits

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
    PR_REMOVE_AND_INIT_LINK(entry->GetListNode());
    PR_APPEND_LINK(entry->GetListNode(), &mEvictionList);
    
    return entry;;
}


nsresult
nsMemoryCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    if (entry->IsDoomed()) {
#if debug
        //** verify we've removed it from mMemCacheEntries & eviction list
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
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry->GetListNode()),"entry is already on a list!");

    // append entry to the eviction list
    PR_APPEND_LINK(entry->GetListNode(), &mEvictionList);

    // add entry to hashtable of mem cache entries
    nsresult  rv = mMemCacheEntries.AddEntry(entry);
    if (NS_FAILED(rv)) {
        PR_REMOVE_AND_INIT_LINK(entry->GetListNode());
        return rv;
    }

    //** add size of entry to memory totals
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    nsresult  rv = mMemCacheEntries.RemoveEntry(entry);
    NS_ASSERTION(NS_SUCCEEDED(rv), "dooming entry that we don't have");
    if (NS_FAILED(rv)) return rv;

    // remove entry from our eviction list
    PR_REMOVE_AND_INIT_LINK(entry->GetListNode());

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
    //** keep track of totals
    return NS_OK;
}

//** need methods for enumerating entries


//** check entry->IsInUse() before evicting.
