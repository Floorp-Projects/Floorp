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
#include "nsMemoryCacheTransport.h"
#include "nsICacheService.h"


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

    rv = mInactiveEntries.Init();

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
    nsCacheEntry * entry = mInactiveEntries.GetEntry(key);
    if (!entry)  return nsnull;

    //** need entry->UpdateFrom(ourEntry);
    entry->MarkActive(); // so we don't evict it
    //** find eviction element and move it to the tail of the queue
    
    return entry;;
}


nsresult
nsMemoryCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    nsCString * key = entry->Key();

    nsCacheEntry * ourEntry = mInactiveEntries.GetEntry(key);
    NS_ASSERTION(ourEntry, "DeactivateEntry called for an entry we don't have!");
    if (!ourEntry)
        return NS_ERROR_INVALID_POINTER;

    //** need ourEntry->UpdateFrom(entry);
    //** MarkInactive(); // to make it evictable again
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsMemoryCacheDevice::BindEntry(nsCacheEntry * entry)
{
    nsresult  rv = mInactiveEntries.AddEntry(entry);
    if (NS_FAILED(rv))
        return rv;

    //** add size of entry to memory totals
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::DoomEntry(nsCacheEntry * entry)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
        NS_NEWXPCOM(*transport, nsMemoryCacheTransport);
        if (!*transport)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(*transport);
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
