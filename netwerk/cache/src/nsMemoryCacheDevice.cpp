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


nsMemoryCacheDevice::nsMemoryCacheDevice()
{

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


nsresult
nsMemoryCacheDevice::Create(nsCacheDevice **result)
{
    nsMemoryCacheDevice * device = new nsMemoryCacheDevice();
    if (!device)  return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = device->Init();
    if (NS_FAILED(rv)) {
        delete device;
        device = nsnull;
    }
    *result = device;
    return rv;
}


const char *
nsMemoryCacheDevice::GetDeviceID()
{
    return "memory";
}


nsresult
nsMemoryCacheDevice::ActivateEntryIfFound(nsCacheEntry * entry)
{
    //** get the key from the entry
    nsCString * key = nsnull; //** = key from entry

    nsCacheEntry * ourEntry = mInactiveEntries.GetEntry(key);
    if (!ourEntry) {
        *entry = nsnull;
        return NS_ERROR_CACHE_KEY_NOT_FOUND;
    }

    //** need entry->UpdateFrom(ourEntry);
    entry->MarkActive(); // so we don't evict it
    //** find eviction element and move it to the tail of the queue
    
    return NS_OK;
}


nsresult
nsMemoryCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    nsCString * key;
    entry->GetKey(&key);

    nsCacheEntry * ourEntry = mInactiveEntries.GetEntry(key);
    NS_ASSERTION(ourEntry, "DeactivateEntry called for an entry we don't have!");
    if (!ourEntry)
        return NS_ERROR_INVALID_POINTER;

    //** need ourEntry->UpdateFrom(entry);
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsMemoryCacheDevice::BindEntry(nsCacheEntry * entry)
{
    nsresult  rv = mInactiveEntries.AddEntry(entry);
    if (NS_FAILED(rv))
        return rv;

    //** add size of entry to memory totals
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsMemoryCacheDevice::GetTransportForEntry( nsCacheEntry * entry,
                                           nsITransport **transport )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsMemoryCacheDevice::OnDataSizeChanged( nsCacheEntry * entry )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//** need methods for enumerating entries
