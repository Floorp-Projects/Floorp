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

#include "nsDiskCacheDevice.h"
#include "nsICacheService.h"


nsDiskCacheDevice::nsDiskCacheDevice()
{

}


nsDiskCacheDevice::~nsDiskCacheDevice()
{

}


nsresult
nsDiskCacheDevice::Init()
{
    return  NS_OK;
}


nsresult
nsDiskCacheDevice::Create(nsCacheDevice **result)
{
    nsDiskCacheDevice * device = new nsDiskCacheDevice();
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
nsDiskCacheDevice::GetDeviceID()
{
    return "disk";
}


nsresult
nsDiskCacheDevice::ActivateEntryIfFound(nsCacheEntry * entry)
{
    return  NS_ERROR_CACHE_KEY_NOT_FOUND;
}


nsresult
nsDiskCacheDevice::DeactivateEntry(nsCacheEntry * entry)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsDiskCacheDevice::BindEntry(nsCacheEntry * entry)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDiskCacheDevice::GetTransportForEntry( nsCacheEntry * entry,
                                           nsITransport **transport )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDiskCacheDevice::OnDataSizeChanged( nsCacheEntry * entry )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//** need methods for enumerating entries
